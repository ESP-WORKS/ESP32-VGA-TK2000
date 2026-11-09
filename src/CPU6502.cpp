#include "CPU6502.h"
#include "TKUniversalPREAD.h"
#include "TKSaturn128K.h"
#include "Bus.h"
#include "TKVideoDirty.h"
#include "BuildConfig.h"
// Resolve mnemonics to compact operation IDs at compile time. The old core
// performed a chain of strcmp() calls for almost every 6502 instruction.
enum OpId : uint8_t { OP_BRK, OP_ORA, OP_NOP, OP_PHP, OP_ASL, OP_BPL, OP_CLC,
 OP_JSR, OP_AND, OP_BIT, OP_ROL, OP_PLP, OP_BMI, OP_SEC, OP_RTI, OP_EOR,
 OP_LSR, OP_PHA, OP_BVC, OP_CLI, OP_RTS, OP_ADC, OP_ROR, OP_BVS, OP_SEI,
 OP_STA, OP_STY, OP_STX, OP_DEY, OP_TXA, OP_BCC, OP_TYA, OP_TXS, OP_LDY,
 OP_LDA, OP_LDX, OP_TAY, OP_TAX, OP_BCS, OP_CLV, OP_TSX, OP_CPY, OP_DEC,
 OP_INY, OP_DEX, OP_BNE, OP_CLD, OP_CPX, OP_INC, OP_INX, OP_BEQ, OP_SED,
 OP_SBC, OP_JMP, OP_CMP, OP_PLA };
// C++11 (used by ESP32 Arduino core 2.0.17) only permits a single return
// statement in a constexpr function. Keep this mapping constexpr to avoid
// constructing the 256-opcode dispatch table at runtime.
constexpr bool mnemonicEq(const char *a, const char *b) {
  return (*a == *b) && ((*a == '\0') || mnemonicEq(a + 1, b + 1));
}
constexpr uint8_t opId(const char *n) {
  return
    mnemonicEq(n, "BRK") ? OP_BRK :
    mnemonicEq(n, "ORA") ? OP_ORA :
    mnemonicEq(n, "NOP") ? OP_NOP :
    mnemonicEq(n, "PHP") ? OP_PHP :
    mnemonicEq(n, "PLA") ? OP_PLA :
    mnemonicEq(n, "ASL") ? OP_ASL :
    mnemonicEq(n, "BPL") ? OP_BPL :
    mnemonicEq(n, "CLC") ? OP_CLC :
    mnemonicEq(n, "JSR") ? OP_JSR :
    mnemonicEq(n, "AND") ? OP_AND :
    mnemonicEq(n, "BIT") ? OP_BIT :
    mnemonicEq(n, "ROL") ? OP_ROL :
    mnemonicEq(n, "PLP") ? OP_PLP :
    mnemonicEq(n, "BMI") ? OP_BMI :
    mnemonicEq(n, "SEC") ? OP_SEC :
    mnemonicEq(n, "RTI") ? OP_RTI :
    mnemonicEq(n, "EOR") ? OP_EOR :
    mnemonicEq(n, "LSR") ? OP_LSR :
    mnemonicEq(n, "PHA") ? OP_PHA :
    mnemonicEq(n, "BVC") ? OP_BVC :
    mnemonicEq(n, "CLI") ? OP_CLI :
    mnemonicEq(n, "RTS") ? OP_RTS :
    mnemonicEq(n, "ADC") ? OP_ADC :
    mnemonicEq(n, "ROR") ? OP_ROR :
    mnemonicEq(n, "BVS") ? OP_BVS :
    mnemonicEq(n, "SEI") ? OP_SEI :
    mnemonicEq(n, "STA") ? OP_STA :
    mnemonicEq(n, "STY") ? OP_STY :
    mnemonicEq(n, "STX") ? OP_STX :
    mnemonicEq(n, "DEY") ? OP_DEY :
    mnemonicEq(n, "TXA") ? OP_TXA :
    mnemonicEq(n, "BCC") ? OP_BCC :
    mnemonicEq(n, "TYA") ? OP_TYA :
    mnemonicEq(n, "TXS") ? OP_TXS :
    mnemonicEq(n, "LDY") ? OP_LDY :
    mnemonicEq(n, "LDA") ? OP_LDA :
    mnemonicEq(n, "LDX") ? OP_LDX :
    mnemonicEq(n, "TAY") ? OP_TAY :
    mnemonicEq(n, "TAX") ? OP_TAX :
    mnemonicEq(n, "BCS") ? OP_BCS :
    mnemonicEq(n, "CLV") ? OP_CLV :
    mnemonicEq(n, "TSX") ? OP_TSX :
    mnemonicEq(n, "CPY") ? OP_CPY :
    mnemonicEq(n, "CMP") ? OP_CMP :
    mnemonicEq(n, "JMP") ? OP_JMP :
    mnemonicEq(n, "DEC") ? OP_DEC :
    mnemonicEq(n, "INY") ? OP_INY :
    mnemonicEq(n, "DEX") ? OP_DEX :
    mnemonicEq(n, "BNE") ? OP_BNE :
    mnemonicEq(n, "CLD") ? OP_CLD :
    mnemonicEq(n, "CPX") ? OP_CPX :
    mnemonicEq(n, "INC") ? OP_INC :
    mnemonicEq(n, "INX") ? OP_INX :
    mnemonicEq(n, "BEQ") ? OP_BEQ :
    mnemonicEq(n, "SED") ? OP_SED :
    mnemonicEq(n, "SBC") ? OP_SBC :
    OP_NOP;
}
static_assert(opId("BRK") == OP_BRK, "C++11 constexpr opcode lookup failed");
static_assert(opId("PLA") == OP_PLA, "C++11 constexpr opcode lookup failed");
static_assert(opId("SBC") == OP_SBC, "C++11 constexpr opcode lookup failed");

static inline bool flagTest(uint8_t p,uint8_t b){return p&(1<<b);} static inline void SF(uint8_t &p,uint8_t b,bool v){if(v)p|=1<<b;else p&=~(1<<b);}
enum {IMM, ZP, ZPX, ZPY, ABS, ABSX, ABSY, IND, INDX, INDY, REL, ACC, IMP};
struct Op{uint8_t mode,cyc,id;};
#define O(n,m,c) {m,c,opId(n)}
static constexpr Op T[256]={
O("BRK",IMP,7),O("ORA",INDX,6),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZP,3),O("ORA",ZP,3),O("ASL",ZP,5),O("NOP",IMP,2),O("PHP",IMP,3),O("ORA",IMM,2),O("ASL",ACC,2),O("NOP",IMP,2),O("NOP",ABS,4),O("ORA",ABS,4),O("ASL",ABS,6),O("NOP",IMP,2),
O("BPL",REL,2),O("ORA",INDY,5),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZPX,4),O("ORA",ZPX,4),O("ASL",ZPX,6),O("NOP",IMP,2),O("CLC",IMP,2),O("ORA",ABSY,4),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ABSX,4),O("ORA",ABSX,4),O("ASL",ABSX,7),O("NOP",IMP,2),
O("JSR",ABS,6),O("AND",INDX,6),O("NOP",IMP,2),O("NOP",IMP,2),O("BIT",ZP,3),O("AND",ZP,3),O("ROL",ZP,5),O("NOP",IMP,2),O("PLP",IMP,4),O("AND",IMM,2),O("ROL",ACC,2),O("NOP",IMP,2),O("BIT",ABS,4),O("AND",ABS,4),O("ROL",ABS,6),O("NOP",IMP,2),
O("BMI",REL,2),O("AND",INDY,5),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZPX,4),O("AND",ZPX,4),O("ROL",ZPX,6),O("NOP",IMP,2),O("SEC",IMP,2),O("AND",ABSY,4),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ABSX,4),O("AND",ABSX,4),O("ROL",ABSX,7),O("NOP",IMP,2),
O("RTI",IMP,6),O("EOR",INDX,6),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZP,3),O("EOR",ZP,3),O("LSR",ZP,5),O("NOP",IMP,2),O("PHA",IMP,3),O("EOR",IMM,2),O("LSR",ACC,2),O("NOP",IMP,2),O("JMP",ABS,3),O("EOR",ABS,4),O("LSR",ABS,6),O("NOP",IMP,2),
O("BVC",REL,2),O("EOR",INDY,5),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZPX,4),O("EOR",ZPX,4),O("LSR",ZPX,6),O("NOP",IMP,2),O("CLI",IMP,2),O("EOR",ABSY,4),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ABSX,4),O("EOR",ABSX,4),O("LSR",ABSX,7),O("NOP",IMP,2),
O("RTS",IMP,6),O("ADC",INDX,6),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZP,3),O("ADC",ZP,3),O("ROR",ZP,5),O("NOP",IMP,2),O("PLA",IMP,4),O("ADC",IMM,2),O("ROR",ACC,2),O("NOP",IMP,2),O("JMP",IND,5),O("ADC",ABS,4),O("ROR",ABS,6),O("NOP",IMP,2),
O("BVS",REL,2),O("ADC",INDY,5),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZPX,4),O("ADC",ZPX,4),O("ROR",ZPX,6),O("NOP",IMP,2),O("SEI",IMP,2),O("ADC",ABSY,4),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ABSX,4),O("ADC",ABSX,4),O("ROR",ABSX,7),O("NOP",IMP,2),
O("NOP",IMM,2),O("STA",INDX,6),O("NOP",IMM,2),O("NOP",IMP,2),O("STY",ZP,3),O("STA",ZP,3),O("STX",ZP,3),O("NOP",IMP,2),O("DEY",IMP,2),O("NOP",IMM,2),O("TXA",IMP,2),O("NOP",IMP,2),O("STY",ABS,4),O("STA",ABS,4),O("STX",ABS,4),O("NOP",IMP,2),
O("BCC",REL,2),O("STA",INDY,6),O("NOP",IMP,2),O("NOP",IMP,2),O("STY",ZPX,4),O("STA",ZPX,4),O("STX",ZPY,4),O("NOP",IMP,2),O("TYA",IMP,2),O("STA",ABSY,5),O("TXS",IMP,2),O("NOP",IMP,2),O("NOP",IMP,5),O("STA",ABSX,5),O("NOP",IMP,2),O("NOP",IMP,2),
O("LDY",IMM,2),O("LDA",INDX,6),O("LDX",IMM,2),O("NOP",IMP,2),O("LDY",ZP,3),O("LDA",ZP,3),O("LDX",ZP,3),O("NOP",IMP,2),O("TAY",IMP,2),O("LDA",IMM,2),O("TAX",IMP,2),O("NOP",IMP,2),O("LDY",ABS,4),O("LDA",ABS,4),O("LDX",ABS,4),O("NOP",IMP,2),
O("BCS",REL,2),O("LDA",INDY,5),O("NOP",IMP,2),O("NOP",IMP,2),O("LDY",ZPX,4),O("LDA",ZPX,4),O("LDX",ZPY,4),O("NOP",IMP,2),O("CLV",IMP,2),O("LDA",ABSY,4),O("TSX",IMP,2),O("NOP",IMP,2),O("LDY",ABSX,4),O("LDA",ABSX,4),O("LDX",ABSY,4),O("NOP",IMP,2),
O("CPY",IMM,2),O("CMP",INDX,6),O("NOP",IMM,2),O("NOP",IMP,2),O("CPY",ZP,3),O("CMP",ZP,3),O("DEC",ZP,5),O("NOP",IMP,2),O("INY",IMP,2),O("CMP",IMM,2),O("DEX",IMP,2),O("NOP",IMP,2),O("CPY",ABS,4),O("CMP",ABS,4),O("DEC",ABS,6),O("NOP",IMP,2),
O("BNE",REL,2),O("CMP",INDY,5),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZPX,4),O("CMP",ZPX,4),O("DEC",ZPX,6),O("NOP",IMP,2),O("CLD",IMP,2),O("CMP",ABSY,4),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ABSX,4),O("CMP",ABSX,4),O("DEC",ABSX,7),O("NOP",IMP,2),
O("CPX",IMM,2),O("SBC",INDX,6),O("NOP",IMM,2),O("NOP",IMP,2),O("CPX",ZP,3),O("SBC",ZP,3),O("INC",ZP,5),O("NOP",IMP,2),O("INX",IMP,2),O("SBC",IMM,2),O("NOP",IMP,2),O("NOP",IMP,2),O("CPX",ABS,4),O("SBC",ABS,4),O("INC",ABS,6),O("NOP",IMP,2),
O("BEQ",REL,2),O("SBC",INDY,5),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ZPX,4),O("SBC",ZPX,4),O("INC",ZPX,6),O("NOP",IMP,2),O("SED",IMP,2),O("SBC",ABSY,4),O("NOP",IMP,2),O("NOP",IMP,2),O("NOP",ABSX,4),O("SBC",ABSX,4),O("INC",ABSX,7),O("NOP",IMP,2)
};
#undef O
// Both paths are kept for A/B performance and bus-equivalence testing.
// ESP32 default uses the fast path, but avoids caching the overlay *value*:
// C05A/C05B bank switches take effect on the very next CPU access.
uint8_t CPU6502::rd(uint16_t a) {
#if TK2000_CPU_FAST_MEMORY
  if (fastRAM) {
    if (a < 0xC000u) return fastRAM[a];
    if (a >= 0xC200u && fastROM && fastROMSelected) {
      if (a >= 0xD000u && *fastROMSelected && fastSaturn && fastSaturn->readEnabled())
        return fastSaturn->read(a);
      return *fastROMSelected ? fastROM[a-0xC000u] : fastRAM[a];
    }
  }
#endif
  return bus->read(a);
}
void CPU6502::wr(uint16_t a, uint8_t v) {
#if TK2000_CPU_FAST_MEMORY
  if (fastRAM) {
    if (a < 0xC000u) {
      fastRAM[a]=v;
      if (fastVideoDirty && ((a>=0x2000u && a<0x4000u) || a>=0xA000u))
        fastVideoDirty->mark(a);
      return;
    }
    if (a >= 0xC200u && fastROMSelected) {
      if (a >= 0xD000u && *fastROMSelected && fastSaturn && fastSaturn->writeEnabled()) {
        fastSaturn->write(a,v);
      } else if (!*fastROMSelected) fastRAM[a]=v;
      return;
    }
  }
#endif
  bus->write(a,v);
} uint16_t CPU6502::rw(uint16_t a){uint8_t l=rd(a),h=rd((uint16_t)(a+1));return l|(h<<8);} void CPU6502::setZN(uint8_t v){SF(P,1,v==0);SF(P,7,v&0x80);} uint8_t CPU6502::fetch(){return rd(PC++);}
uint16_t CPU6502::addr(int m){
 switch(m){
  case IMM:return PC++;
  case ZP:return fetch();
  case ZPX:return(uint8_t)(fetch()+X);
  case ZPY:return(uint8_t)(fetch()+Y);
  case ABS:{uint8_t lo=fetch();uint8_t hi=fetch();return(uint16_t)(lo|((uint16_t)hi<<8));}
  case ABSX:{uint8_t lo=fetch();uint8_t hi=fetch();uint16_t b=(uint16_t)(lo|((uint16_t)hi<<8));uint16_t a=(uint16_t)(b+X);pageCrossed=((b&0xFF00)!=(a&0xFF00));return a;}
  case ABSY:{uint8_t lo=fetch();uint8_t hi=fetch();uint16_t b=(uint16_t)(lo|((uint16_t)hi<<8));uint16_t a=(uint16_t)(b+Y);pageCrossed=((b&0xFF00)!=(a&0xFF00));return a;}
  case INDX:{uint8_t z=(uint8_t)(fetch()+X);uint8_t lo=rd(z);uint8_t hi=rd((uint8_t)(z+1));return(uint16_t)(lo|((uint16_t)hi<<8));}
  case INDY:{uint8_t z=fetch();uint8_t lo=rd(z);uint8_t hi=rd((uint8_t)(z+1));uint16_t b=(uint16_t)(lo|((uint16_t)hi<<8));uint16_t a=(uint16_t)(b+Y);pageCrossed=((b&0xFF00)!=(a&0xFF00));return a;}
  case IND:{uint8_t lo=fetch();uint8_t hi=fetch();uint16_t p=(uint16_t)(lo|((uint16_t)hi<<8));uint8_t targetLo=rd(p);uint16_t next=(uint16_t)((p&0xFF00)|((p+1)&0x00FF));uint8_t targetHi=rd(next);return(uint16_t)(targetLo|((uint16_t)targetHi<<8));}
  default:return 0;
 }
}
void CPU6502::service(uint16_t v,bool brk){wr(0x100+S--,PC>>8);wr(0x100+S--,PC);wr(0x100+S--,(uint8_t)((P&~0x10)|(brk?0x10:0)|0x20));P|=4;PC=rw(v);}
void CPU6502::reset(){P=0x24;S=0xFD;PC=rw(0xFFFC);cycles=7;}
// Same selective hot-function optimization used by MSX v38.9.  Avoid
// enabling O3 globally: FabGL/SD cold paths stay size-optimized for I-cache.
#if TK2000_CPU_HOT_O3 && defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
int CPU6502::step(){
 if(nmi_pending){nmi_pending=false;service(0xFFFA);cycles+=7;return 7;}
 if(irq_pending&&!flagTest(P,2)){service(0xFFFE);cycles+=7;return 7;}
 // Some legacy TKDOS images call the Apple-compatible entry $FB2F from
 // different bootstrap addresses. In this verified TK2000 ROM, $FB2F is in
 // the middle of JMP $F117 located at $FB2E. The old single-$0845 workaround
 // fixed only one disk revision. Accept ONLY genuine guest JSR $FB2F calls
 // originating in writable RAM, verified by the 6502 stack return address
 // and the three JSR opcode bytes. Never alter ROMs or disk image bytes.
 if (tkdosLegacyBootAlias && PC==0xFB2F) {
   const uint16_t ret = (uint16_t)(rd((uint16_t)(0x100u+(uint8_t)(S+1))) |
                        ((uint16_t)rd((uint16_t)(0x100u+(uint8_t)(S+2)))<<8));
   if (ret>=0x0202 && ret<0xC000 &&
       rd((uint16_t)(ret-2))==0x20 &&
       rd((uint16_t)(ret-1))==0x2F && rd(ret)==0xFB) PC=0xFB2E;
 }
#if TK2000_UNIVERSAL_PREAD
 // JOYSTICK ON = an Apple-compatible analog joystick is attached (FPGA
 // TK2000 II gameport): every genuine JSR $FB1E (Apple PREAD entry) gets
 // the axis value, for any program. JOYSTICK OFF = nothing attached: the
 // untouched TK ROM runs (SEC;RTS) and Y is preserved, as on a real TK.
 if (universalPreadActive && TKUniversalPREADCall(fastRAM,fastROM,
       fastROMSelected && *fastROMSelected,PC,X,S))
   Y=(X==0u)?universalPreadX:universalPreadY;
#endif
#if TK2000_GAME_JOYSTICK_COMPAT
 // Moon Patrol contains JSR $FB1E at $0766 but on this TK ROM that is
 // SEC/RTS instead of Apple II PREAD. Confirm the *caller's* literal JSR,
 // stack return, live ROM overlay and ROM stub before changing Y. This
 // neither patches guest RAM/ROM nor affects normal emulation cycles.
 if (moonPatrolPreadCompat && PC==0xFB1Eu && X<2u && fastRAM && fastROM &&
     fastROMSelected && *fastROMSelected &&
     fastROM[0x3B1Eu]==0x38u && fastROM[0x3B1Fu]==0x60u &&
     fastRAM[0x0766u]==0x20u && fastRAM[0x0767u]==0x1Eu &&
     fastRAM[0x0768u]==0xFBu &&
     // JSR pushes the address of its LAST operand byte ($0768),
     // not the following instruction ($0769). v0.83 used the wrong
     // guest JSR start ($0765) and return ($0767), so it never activated.
     fastRAM[0x100u+(uint8_t)(S+1u)]==0x68u &&
     fastRAM[0x100u+(uint8_t)(S+2u)]==0x07u)
   Y=(X==0u)?moonPatrolPreadX:moonPatrolPreadY;
 // Missiles Defense (CT2 CRC32 43B19501) calls the Apple PREAD entry
 // literally from $41C3. The verified TK ROM has SEC/RTS there; supply
 // just the original contract (coordinate in Y), keeping RTS/stack/cycles.
 // Validate live ROM, the JSR instruction and the actual stacked return.
 if (missilesDefensePreadCompat && PC==0xFB1Eu && X<2u && fastRAM && fastROM &&
     fastROMSelected && *fastROMSelected &&
     fastROM[0x3B1Eu]==0x38u && fastROM[0x3B1Fu]==0x60u) {
   // Verify a genuine JSR inside the loaded game's address window. This
   // also tolerates game self-modification of alternate axis call sites;
   // no arbitrary ROM or unrelated application's PREAD calls are changed.
   const uint16_t ret=(uint16_t)(fastRAM[0x100u+(uint8_t)(S+1u)] |
                      ((uint16_t)fastRAM[0x100u+(uint8_t)(S+2u)]<<8));
   if (ret>=0x4002u && ret<=0x634Eu &&
       fastRAM[ret-2u]==0x20u && fastRAM[ret-1u]==0x1Eu && fastRAM[ret]==0xFBu)
     Y=(X==0u)?missilesDefensePreadX:missilesDefensePreadY;
 }
#endif // TK2000_GAME_JOYSTICK_COMPAT
 pageCrossed=false;
 uint8_t o=fetch(); const Op &q=T[o]; uint16_t a=0; uint8_t v=0,r=0; int c=q.cyc;
 auto readOperand=[&](){ if(q.mode==IMM) return fetch(); a=addr(q.mode); if(pageCrossed) ++c; return rd(a); };
 if(q.id==OP_NOP){
   // Known NMOS unofficial NOPs still fetch operands. Memory forms also
   // perform the data read, and absolute,X incurs a page-crossing cycle.
   // Treating every unknown byte as a one-byte NOP desynchronized the PC.
   if(q.mode==IMM) (void)fetch();
   else if(q.mode!=IMP) {a=addr(q.mode);(void)rd(a);if(q.mode==ABSX && pageCrossed)++c;}
 }
 else if(q.id==OP_LDA){A=readOperand();setZN(A);}
 else if(q.id==OP_LDX){X=readOperand();setZN(X);}
 else if(q.id==OP_LDY){Y=readOperand();setZN(Y);}
 else if(q.id==OP_STA){a=addr(q.mode);wr(a,A);}
 else if(q.id==OP_STX){a=addr(q.mode);wr(a,X);}
 else if(q.id==OP_STY){a=addr(q.mode);wr(a,Y);}
 else if(q.id==OP_TAX){X=A;setZN(X);}
 else if(q.id==OP_TAY){Y=A;setZN(Y);}
 else if(q.id==OP_TXA){A=X;setZN(A);}
 else if(q.id==OP_TYA){A=Y;setZN(A);}
 else if(q.id==OP_TSX){X=S;setZN(X);}
 else if(q.id==OP_TXS){S=X;}
 else if(q.id==OP_PHA){wr(0x100+S--,A);}
 else if(q.id==OP_PHP){wr(0x100+S--,P|0x30);}
 else if(q.id==OP_PLA){A=rd(0x100+(uint8_t)++S);setZN(A);}
 else if(q.id==OP_PLP){P=rd(0x100+(uint8_t)++S);P|=0x20;}
 else if(q.id==OP_INX){X++;setZN(X);}
 else if(q.id==OP_INY){Y++;setZN(Y);}
 else if(q.id==OP_DEX){X--;setZN(X);}
 else if(q.id==OP_DEY){Y--;setZN(Y);}
 else if(q.id==OP_CLC)SF(P,0,0);
 else if(q.id==OP_SEC)SF(P,0,1);
 else if(q.id==OP_CLI)SF(P,2,0);
 else if(q.id==OP_SEI)SF(P,2,1);
 else if(q.id==OP_CLV)SF(P,6,0);
 else if(q.id==OP_CLD)SF(P,3,0);
 else if(q.id==OP_SED)SF(P,3,1);
 else if(q.id==OP_JMP){PC=addr(q.mode);}
 else if(q.id==OP_JSR){uint16_t t=addr(ABS);uint16_t ret=PC-1;wr(0x100+S--,ret>>8);wr(0x100+S--,ret);PC=t;}
 else if(q.id==OP_RTS){uint8_t lo=rd(0x100+(uint8_t)++S);uint8_t hi=rd(0x100+(uint8_t)++S);PC=(uint16_t)((lo|(hi<<8))+1);}
 else if(q.id==OP_RTI){P=rd(0x100+(uint8_t)++S);P|=0x20;uint8_t lo=rd(0x100+(uint8_t)++S);uint8_t hi=rd(0x100+(uint8_t)++S);PC=(uint16_t)(lo|(hi<<8));}
 else if(q.id==OP_BRK){PC++;service(0xFFFE,true);}
 else if(q.mode==REL && (q.id==OP_BPL || q.id==OP_BMI || q.id==OP_BVC || q.id==OP_BVS || q.id==OP_BCC || q.id==OP_BCS || q.id==OP_BNE || q.id==OP_BEQ)){
   int8_t d=(int8_t)fetch(); bool take=false;
   if(q.id==OP_BPL) take=!flagTest(P,7);
   if(q.id==OP_BMI) take=flagTest(P,7);
   if(q.id==OP_BVC) take=!flagTest(P,6);
   if(q.id==OP_BVS) take=flagTest(P,6);
   if(q.id==OP_BCC) take=!flagTest(P,0);
   if(q.id==OP_BCS) take=flagTest(P,0);
   if(q.id==OP_BNE) take=!flagTest(P,1);
   if(q.id==OP_BEQ) take=flagTest(P,1);
   if(take){uint16_t oldPC=PC;PC=(uint16_t)(PC+d);c++;if((oldPC&0xFF00)!=(PC&0xFF00))c++;}
 }
 else if(q.id==OP_ASL||q.id==OP_LSR||q.id==OP_ROL||q.id==OP_ROR){
   if(q.mode==ACC){v=A;} else {a=addr(q.mode);v=rd(a);}
   if(q.id==OP_ASL){SF(P,0,v&0x80);r=(uint8_t)(v<<1);}
   else if(q.id==OP_LSR){SF(P,0,v&1);r=(uint8_t)(v>>1);}
   else if(q.id==OP_ROL){bool c0=flagTest(P,0);SF(P,0,v&0x80);r=(uint8_t)((v<<1)|(c0?1:0));}
   else {bool c0=flagTest(P,0);SF(P,0,v&1);r=(uint8_t)((v>>1)|(c0?0x80:0));}
   setZN(r);
   if(q.mode==ACC) A=r;
   else {
     // NMOS RMW: the dummy write matters for I/O soft-switches. Regular
     // RAM/ROM needs only the final value; avoid another costly bus call.
     if((a&0xFF00u)==0xC000u) wr(a,v);
     wr(a,r);
   }
 }
 else if(q.id==OP_INC||q.id==OP_DEC){
   a=addr(q.mode);v=rd(a);r=q.id==OP_INC?(uint8_t)(v+1):(uint8_t)(v-1);
   if((a&0xFF00u)==0xC000u) wr(a,v); // I/O dummy write / soft-switch
   wr(a,r);setZN(r);
 }
 else {
   v=readOperand();
   if(q.id==OP_ORA)r=A|v;
   else if(q.id==OP_AND)r=A&v;
   else if(q.id==OP_EOR)r=A^v;
   else if(q.id==OP_ADC||q.id==OP_SBC){
     const bool isADC = q.id==OP_ADC;
     const uint8_t carryIn = flagTest(P,0) ? 1 : 0;
     uint8_t binaryResult;
     uint16_t binaryWide;
     if(isADC){
       binaryWide=(uint16_t)A+(uint16_t)v+carryIn;
       binaryResult=(uint8_t)binaryWide;
       SF(P,0,binaryWide>0xFF);
       SF(P,6,((~(A^v)&(A^binaryResult)&0x80)!=0));
       r=binaryResult;
       if(flagTest(P,3)){
         uint16_t lo=(A&0x0F)+(v&0x0F)+carryIn;
         if(lo>9) lo+=6;
         uint16_t hi=(A>>4)+(v>>4)+(lo>0x0F);
         if(hi>9) hi+=6;
         SF(P,0,hi>0x0F);
         // hi can be negative after BCD borrow. Shifting a negative signed
         // integer is undefined C++; mask into an unsigned nibble first.
         r=(uint8_t)((((uint16_t)hi & 0x0Fu)<<4)|((uint16_t)lo&0x0Fu));
       }
     } else {
       int16_t diff=(int16_t)A-(int16_t)v-(carryIn?0:1);
       binaryWide=(uint16_t)diff;
       binaryResult=(uint8_t)diff;
       SF(P,0,diff>=0);
       SF(P,6,(((A^v)&(A^binaryResult)&0x80)!=0));
       r=binaryResult;
       if(flagTest(P,3)){
         int16_t lo=(A&0x0F)-(v&0x0F)-(carryIn?0:1);
         int16_t hi=(A>>4)-(v>>4);
         if(lo<0){lo-=6;--hi;}
         if(hi<0)hi-=6;
         // hi can be negative after BCD borrow. Shifting a negative signed
         // integer is undefined C++; mask into an unsigned nibble first.
         r=(uint8_t)((((uint16_t)hi & 0x0Fu)<<4)|((uint16_t)lo&0x0Fu));
       }
     }
     // On the NMOS 6502, N/Z reflect the binary intermediate result in
     // decimal mode; D itself is never modified by ADC/SBC.
     setZN(flagTest(P,3) ? binaryResult : r);
   }
   else if(q.id==OP_CMP||q.id==OP_CPX||q.id==OP_CPY){
     uint8_t z=q.id==OP_CMP?A:(q.id==OP_CPX?X:Y);uint8_t d=(uint8_t)(z-v);SF(P,0,z>=v);setZN(d);
   }
   else if(q.id==OP_BIT){SF(P,1,(A&v)==0);SF(P,7,v&0x80);SF(P,6,v&0x40);}
   if(q.id==OP_ORA||q.id==OP_AND||q.id==OP_EOR){A=r;setZN(A);}
   else if(q.id==OP_ADC||q.id==OP_SBC){A=r;if(!flagTest(P,3))setZN(A);}
 }
 cycles+=c;return c;
}
