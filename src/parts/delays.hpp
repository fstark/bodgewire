#pragma once

#include "sim/primitives.hpp"

// TI datasheet typical values, 5 V, 25 °C, CL = 15 pF; picoseconds.
namespace bw::delays {

inline constexpr Delays kLs00{9'000, 10'000};
inline constexpr Delays kLs02{10'000, 10'000};
inline constexpr Delays kLs04{9'000, 10'000};
inline constexpr Delays kLs08{8'000, 10'000};
inline constexpr Delays kLs20{9'000, 10'000};
inline constexpr Delays kLs27{10'000, 10'000};
inline constexpr Delays kLs30{8'000, 13'000};
inline constexpr Delays kLs32{14'000, 14'000};
inline constexpr Delays kLs86{12'000, 10'000};         // other input low
inline constexpr Delays kLs47{50'000, 50'000};         // data → output; TI gives only 100 ns max
inline constexpr Delays kLs48{50'000, 50'000};         // data → output; as the 74LS47
inline constexpr Delays kLs74{13'000, 25'000};         // CLK, PRE_N, CLR_N → Q, Q_N
inline constexpr Delays kLs107{15'000, 15'000};        // CLK, CLR_N → Q, Q_N
inline constexpr Delays kLs157Data{9'000, 9'000};      // A, B → Y
inline constexpr Delays kLs157Select{15'000, 18'000};  // S → Y
inline constexpr Delays kLs157Strobe{13'000, 14'000};  // G_N → Y
inline constexpr Delays kLs161ClockToQ{13'000, 15'000};
inline constexpr Delays kLs161Clear{20'000, 20'000};  // only fall is used
inline constexpr Delays kLs161ClockToRco{20'000, 18'000};
inline constexpr Delays kLs161EntToRco{9'000, 9'000};
inline constexpr Delays kLs90CkaToQA{10'000, 12'000};
inline constexpr Delays kLs90CkbToQB{10'000, 14'000};
inline constexpr Delays kLs90CkbToQC{21'000, 23'000};
inline constexpr Delays kLs90CkbToQD{21'000, 23'000};
inline constexpr Delays kLs90Clear{26'000, 26'000};    // only fall is used
inline constexpr Delays kLs90SetNine{20'000, 26'000};  // QA, QD rise; QB, QC fall
inline constexpr Delays kLs93CkaToQA{10'000, 12'000};
inline constexpr Delays kLs93CkbToQB{10'000, 14'000};
inline constexpr Delays kLs93CkbToQC{21'000, 23'000};
inline constexpr Delays kLs93CkbToQD{34'000, 34'000};
inline constexpr Delays kLs93Clear{26'000, 26'000};  // only fall is used
inline constexpr Delays kLs164ClockToQ{17'000, 21'000};
inline constexpr Delays kLs164Clear{24'000, 24'000};  // only fall is used
inline constexpr Delays kLs165Clock{21'000, 24'000};  // CLK, CLK_INH → QH
inline constexpr Delays kLs165Load{22'000, 27'000};   // SH_LD_N → QH
inline constexpr Delays kLs165Data{14'000, 21'000};   // H → QH while loading
inline constexpr Delays kLs175ClockToQ{20'000, 21'000};
inline constexpr Delays kLs175Clear{16'000, 20'000};  // Q_N rises, Q falls
// 74LS138 (TI typicals; rise = output to 1).
inline constexpr Delays kLs138Select{21'000, 21'000};    // A, B, C → Y (3 levels)
inline constexpr Delays kLs138EnableG2{12'000, 20'000};  // G2A_N, G2B_N → Y
inline constexpr Delays kLs138EnableG1{17'000, 26'000};  // G1 → Y
// 74LS273 (TI typicals).
inline constexpr Delays kLs273ClockToQ{17'000, 18'000};
inline constexpr Delays kLs273Clear{18'000, 18'000};  // Q falls

// ROMs: one delay for both edges. Intel 2716: maximums (no typicals published).
inline constexpr Time k2716Access = 450'000;        // address → O
inline constexpr Time k2716ChipEnable = 450'000;    // CE_N → O
inline constexpr Time k2716OutputEnable = 120'000;  // OE_N → O
inline constexpr Time k2716Float = 100'000;         // CE_N or OE_N high → O off
// 74LS244 (rise = from/to 1, fall = from/to 0).
inline constexpr Delays kLs244Data{12'000, 12'000};     // A → Y
inline constexpr Delays kLs244Enable{15'000, 20'000};   // OE_N low → Y (tPZH, tPZL)
inline constexpr Delays kLs244Disable{10'000, 15'000};  // OE_N high → Y off (tPHZ, tPLZ)
// Intel 2114 (450 ns grade): maximums (no typicals published).
inline constexpr Time k2114Address = 450'000;  // tAA, address → IO
inline constexpr Time k2114Select = 120'000;   // tCO, CS_N low (or WE_N back high) → IO
inline constexpr Time k2114Off = 100'000;      // tOTD / tWZ, CS_N high or WE_N low → IO off
// Signetics 2513, 1972 MOS data book p. 67: typicals.
inline constexpr Time k2513RowAccess = 450'000;        // A1-A3 → O
inline constexpr Time k2513CharacterAccess = 500'000;  // A4-A9 → O
inline constexpr Time k2513ChipEnable = 150'000;       // CE_N → O, on and off
// Signetics 2518, 1972 MOS data book p. 82: the typical (maximum 350 ns).
inline constexpr Time k2518ClockToOutput = 300'000;  // tA, CLK → OUT
// Signetics 2524, 1972 MOS data book p. 95: the maximum (no typical given).
inline constexpr Time k2524ClockToOutput = 100'000;  // tA+, PHI1 → OUT
// The 2524's bits with a clock stopped: kept 10 ms, then faded (P4q; the datasheet guarantees
// about 1 ms at 25 °C, typically 100 times more).
inline constexpr Time k2524Hold = 10'000'000'000;

// Z80A (4 MHz), Zilog datasheet AC characteristics: maxima, the only figures given. Used by the
// Z80STUB benchmark Part (P5a).
inline constexpr Time kZ80Address = 110'000;  // tD(AD), CLK rise → A0-A15
inline constexpr Time kZ80Mreq = 85'000;      // CLK → MREQ_N, both ways
inline constexpr Time kZ80Rd = 85'000;        // CLK → RD_N, both ways
inline constexpr Time kZ80Wr = 80'000;        // CLK → WR_N, both ways
inline constexpr Time kZ80Iorq = 75'000;      // CLK → IORQ_N, both ways
inline constexpr Time kZ80M1 = 100'000;       // CLK rise → M1_N, both ways
inline constexpr Time kZ80Rfsh = 130'000;     // CLK rise → RFSH_N, both ways
inline constexpr Time kZ80Halt = 300'000;     // CLK → HALT_N
inline constexpr Time kZ80Data = 150'000;     // CLK → D0-D7 valid, or floating

// MOS 6502, 1 MHz (MOS 6500 data sheet, Nov. 1985, p. 5), from the PHI0 edge: the data sheet
// measures from PHI2 (out), whose delay from PHI0 it does not publish (30 ns assumed; PHI1 rises
// after PHI2 falls and falls before it rises: TD >= 5 ns), plus its typical where it gives one.
inline constexpr Delays kMos6502Phi1{40'000, 20'000};
inline constexpr Delays kMos6502Phi2{30'000, 30'000};
inline constexpr Time kMos6502Address = 130'000;    // + tADS 100 ns typical
inline constexpr Time kMos6502ReadWrite = 130'000;  // + tRWS 100 ns typical
inline constexpr Time kMos6502Sync = 390'000;       // + tSYNC 360 ns maximum (no typical)
inline constexpr Time kMos6502DataOut = 180'000;    // PHI0 rise + tMDS 150 ns typical
inline constexpr Time kMos6502DataHold = 90'000;    // PHI0 fall + tHW 60 ns typical: released

}  // namespace bw::delays
