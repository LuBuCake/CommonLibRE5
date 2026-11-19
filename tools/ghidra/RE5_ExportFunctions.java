// Exports every function and every class vtable of re5dx9 for CommonLibRE5's function database.
// Writes <outdir>/functions.tsv and <outdir>/vtables.tsv (addresses are VAs, hex). Read-only: changes nothing.
//   functions.tsv: va, size, name, namespace, source, callconv, purge, thunk_target, xrefs
//   vtables.tsv:   class, vtable label, vtable va, slot, target va, flags
// A vtable is walked until a sure end: a labelled (non-default) symbol, the vtable of an MtDTI, or a pointer outside
// executable memory. Possible ends are flagged and left to tools/gen_functions.py, which knows the class tree (a
// derived vtable is at least as long as its base): "s" = the slot address is stored by code (`mov [reg], imm32`: the
// start of another vtable, or a flag constant that looks like one), "c" = the target is not the start of code.
// Usage: args = output directory.
// @category RE5

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.lang.OperandType;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SymbolType;

import java.io.File;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Set;

public class RE5_ExportFunctions extends GhidraScript
{
    @Override
    public void run() throws Exception
    {
        String[] Args = getScriptArgs();
        File Dir = new File(Args.length > 0 ? String.join(" ", Args) : askDirectory("Output", "Export").getPath());
        Dir.mkdirs();

        FunctionManager Fm = currentProgram.getFunctionManager();
        ReferenceManager Rm = currentProgram.getReferenceManager();
        SymbolTable St = currentProgram.getSymbolTable();
        Memory Mem = currentProgram.getMemory();

        int Count = 0;
        try (PrintWriter Out = new PrintWriter(new File(Dir, "functions.tsv"), "UTF-8"))
        {
            Out.println("va\tsize\tname\tnamespace\tsource\tcallconv\tpurge\tthunk_target\txrefs");
            for (Function F : Fm.getFunctions(true))
            {
                if (F.isExternal())
                {
                    continue;
                }
                String Ns = F.getParentNamespace().isGlobal() ? "" : F.getParentNamespace().getName(true);
                Function Thunked = F.isThunk() ? F.getThunkedFunction(false) : null;
                String ThunkTarget = Thunked != null && !Thunked.isExternal() ? Thunked.getEntryPoint().toString() : "";
                int Purge = F.getStackPurgeSize();
                Out.println(F.getEntryPoint() + "\t" + F.getBody().getNumAddresses() + "\t" + F.getName() + "\t" + Ns + "\t" +
                    F.getSymbol().getSource() + "\t" + F.getCallingConventionName() + "\t" +
                    (Purge == Function.UNKNOWN_STACK_DEPTH_CHANGE || Purge == Function.INVALID_STACK_DEPTH_CHANGE ? "" : Integer.toString(Purge)) +
                    "\t" + ThunkTarget + "\t" + Rm.getReferenceCountTo(F.getEntryPoint()));
                Count++;
            }
        }
        println("functions: " + Count);

        // Raw `mov dword ptr [reg], imm32` (C7 0x) and `mov dword ptr [abs32], imm32` (C7 05) in code: a vtable start
        // stored by a constructor the analysis did not reach (as in RE5_MtDTI.java). Not [reg+disp]: object flags are
        // stored that way and some of them look like addresses inside the vtables (0x1000014...).
        Set<Long> Stored = new HashSet<>();
        for (MemoryBlock B : Mem.getBlocks())
        {
            if (!B.isExecute() || !B.isInitialized())
            {
                continue;
            }
            byte[] Buf = new byte[(int) B.getSize()];
            B.getBytes(B.getStart(), Buf);
            for (int I = 0; I + 7 <= Buf.length; I++)
            {
                if ((Buf[I] & 0xFF) != 0xC7)
                {
                    continue;
                }
                int ModRm = Buf[I + 1] & 0xFF;
                int Imm = ModRm == 5 ? I + 6 : (ModRm <= 7 && ModRm != 4 ? I + 2 : -1);
                if (Imm >= 0 && Imm + 4 <= Buf.length)
                {
                    Stored.add(Read32(Buf, Imm));
                }
            }
        }

        // The vtable of every MtDTI object (its +0x00): DTI vtables sit among the class vtables and have no label.
        Set<Long> DtiVTables = new HashSet<>();
        SymbolIterator Dtis = St.getSymbols("DTI");
        while (Dtis.hasNext())
        {
            DtiVTables.add(Mem.getInt(Dtis.next().getAddress()) & 0xFFFFFFFFL);
        }

        int Tables = 0, Slots = 0;
        try (PrintWriter Out = new PrintWriter(new File(Dir, "vtables.tsv"), "UTF-8"))
        {
            Out.println("class\tlabel\tvtable\tslot\ttarget\tflags");
            SymbolIterator It = St.getAllSymbols(true);
            while (It.hasNext())
            {
                Symbol S = It.next();
                if (!S.getName().startsWith("vftable") || S.getParentNamespace().isGlobal())
                {
                    continue;
                }
                Tables++;
                String Cls = S.getParentNamespace().getName(true);
                Address A = S.getAddress();
                for (int N = 0; N < 1024; N++, A = A.add(4))
                {
                    if (!Mem.contains(A) || N > 0 && (Labelled(St, A) || DtiVTables.contains(A.getOffset())))
                    {
                        break;
                    }
                    long P = Mem.getInt(A) & 0xFFFFFFFFL;
                    Address T = toAddr(P);
                    MemoryBlock B = Mem.getBlock(T);
                    if (B == null || !B.isExecute())
                    {
                        break;
                    }
                    String Flags = (N > 0 && (ReferencedByCode(Rm, A) || Stored.contains(A.getOffset())) ? "s" : "") +
                        (IsCodeStart(Fm, T) ? "" : "c");
                    Out.println(Cls + "\t" + S.getName() + "\t" + S.getAddress() + "\t" + N + "\t" + T + "\t" + Flags);
                    Slots++;
                }
            }
        }
        println("vtables: " + Tables + ", slots: " + Slots);
    }

    // A slot points to a function entry, or to an instruction outside every function (code the analysis has not made a
    // function of). Text after the table ("scs" = 0x00736373) lands mid-instruction or on data.
    boolean IsCodeStart(FunctionManager Fm, Address T)
    {
        if (Fm.getFunctionAt(T) != null)
        {
            return true;
        }
        return currentProgram.getListing().getInstructionAt(T) != null && Fm.getFunctionContaining(T) == null;
    }

    static long Read32(byte[] Buf, int I)
    {
        return (Buf[I] & 0xFFL) | (Buf[I + 1] & 0xFFL) << 8 | (Buf[I + 2] & 0xFFL) << 16 | (Buf[I + 3] & 0xFFL) << 24;
    }

    // Another class's table (vftable/DTI of RE5_MtDTI.java, RTTI symbols): a symbol inside a class namespace. Other
    // analysis labels are not ends (switchD_0096eea0::caseD_4 sits on 0x1030300, a flag value in a jump table).
    static boolean Labelled(SymbolTable St, Address A)
    {
        for (Symbol S : St.getSymbols(A))
        {
            if (S.getSource() != SourceType.DEFAULT && S.getParentNamespace().getSymbol().getSymbolType() == SymbolType.CLASS)
            {
                return true;
            }
        }
        return false;
    }

    // The start of the next vtable is stored by its constructor: `mov dword ptr [reg], imm32` (or into a static object,
    // `mov dword ptr [abs], imm32`). Other references do not
    // end the table: code READS slots (devirtualized `call [vtable+N]`, Ghidra taking `[eax+0x24]` for an address)
    // and flag constants look like addresses (`test [edx], 0xFFF000`, `or [edi+0x520], 0x1000020`, `push 0x1000200`).
    boolean ReferencedByCode(ReferenceManager Rm, Address A)
    {
        for (Reference R : Rm.getReferencesTo(A))
        {
            Instruction I = currentProgram.getListing().getInstructionAt(R.getFromAddress());
            if (I != null && R.getReferenceType().isData() && R.getOperandIndex() == 1 &&
                I.getMnemonicString().equals("MOV") && (I.getOperandType(0) & (OperandType.DYNAMIC | OperandType.ADDRESS)) != 0)
            {
                return true;
            }
        }
        return false;
    }
}
