// Applies CommonLibRE5's function database (data/Functions.tsv, written by tools/gen_functions.py) to the program:
//   - creates the functions Ghidra's analysis missed (vtable targets, MT property getters/setters);
//   - names every function the database names, in the namespace of its class (<class>::<name>).
// Hand names (Names.tsv, VFuncs.tsv) are USER_DEFINED and replace any name; automatic ones (vfNNN, get_/set_ of
// properties) are ANALYSIS and never replace a name given by hand in Ghidra (gen_functions.py takes those into
// Names.tsv on the next run). Rows that came from Ghidra itself are left alone. Signatures go to the plate comment.
// Usage: args = path of Functions.tsv. Run RE5_ExportFunctions.java + gen_functions.py again afterwards.
// @category RE5

import ghidra.app.script.GhidraScript;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.GhidraClass;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;

public class RE5_ApplyNames extends GhidraScript
{
    static final Pattern AUTOMATIC = Pattern.compile("vf\\d{3}|[gs]et_\\w+");
    final Map<String, Namespace> Namespaces = new HashMap<>();

    @Override
    public void run() throws Exception
    {
        String[] Args = getScriptArgs();
        File Db = Args.length > 0 ? new File(String.join(" ", Args)) : askFile("Functions.tsv", "Apply");

        int Created = 0, CreateFailed = 0, Named = 0, KeptUser = 0, Same = 0, Cleared = 0;
        Set<Address> Listed = new HashSet<>();
        try (BufferedReader In = new BufferedReader(new InputStreamReader(new FileInputStream(Db), StandardCharsets.UTF_8)))
        {
            String Line;
            String[] Header = null;
            while ((Line = In.readLine()) != null)
            {
                if (Line.startsWith("#") || Line.isEmpty())
                {
                    continue;
                }
                String[] F = Line.split("\t", -1);
                if (Header == null)
                {
                    Header = F;
                    continue;
                }
                monitor.checkCancelled();
                String Va = F[1], Cls = F[2], Name = F[3], Origin = F[8], Signature = F.length > 9 ? F[9] : "";
                Address A = toAddr(Long.parseLong(Va, 16));
                if (!Name.isEmpty())
                {
                    Listed.add(A);
                }

                Function Fn = getFunctionAt(A);
                if (Fn == null)
                {
                    if (getInstructionAt(A) == null)
                    {
                        disassemble(A);
                    }
                    Fn = getFunctionContaining(A) == null ? createFunction(A, null) : null;
                    if (Fn == null)
                    {
                        CreateFailed++;
                        println("could not create a function at " + A + " (" + Cls + "::" + Name + ")");
                        continue;
                    }
                    Created++;
                }

                if (Name.isEmpty() || Origin.equals("ghidra"))
                {
                    continue;
                }
                boolean Hand = Origin.equals("names") || Origin.equals("slot");
                Namespace Ns = Cls.isEmpty() ? currentProgram.getGlobalNamespace() : Class(Cls);
                if (Fn.getName().equals(Name) && Fn.getParentNamespace().equals(Ns))
                {
                    Same++;
                    continue;
                }
                if (!Hand && Fn.getSymbol().getSource() == SourceType.USER_DEFINED)
                {
                    KeptUser++;
                    continue;
                }
                Fn.setParentNamespace(Ns);
                Fn.setName(Name, Hand ? SourceType.USER_DEFINED : SourceType.ANALYSIS);
                if (!Signature.isEmpty())
                {
                    Fn.setComment(Signature);
                }
                Named++;
            }
        }
        // Automatic names of an earlier run that the database no longer gives (a vtable found shorter, a getter now
        // shared by two properties): back to FUN_.
        for (Function Fn : currentProgram.getFunctionManager().getFunctions(true))
        {
            if (Fn.getSymbol().getSource() == SourceType.ANALYSIS && !Listed.contains(Fn.getEntryPoint()) &&
                AUTOMATIC.matcher(Fn.getName()).matches())
            {
                Fn.setParentNamespace(currentProgram.getGlobalNamespace());
                Fn.setName(null, SourceType.DEFAULT);
                Cleared++;
            }
        }
        println("created " + Created + " functions (" + CreateFailed + " failed), named " + Named + ", already named " + Same +
            ", kept " + KeptUser + " hand names, cleared " + Cleared + " stale automatic names");
    }

    Namespace Class(String Name) throws Exception
    {
        Namespace N = Namespaces.get(Name);
        if (N == null)
        {
            N = NamespaceUtils.createNamespaceHierarchy(Name.replace(' ', '_'), null, currentProgram, SourceType.ANALYSIS);
            if (!(N instanceof GhidraClass))
            {
                N = NamespaceUtils.convertNamespaceToClass(N);
            }
            Namespaces.put(Name, N);
        }
        return N;
    }
}
