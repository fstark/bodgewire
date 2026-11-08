#pragma once

#include "sim/primitives.hpp"

// TI datasheet typical values, 5 V, 25 °C, CL = 15 pF; picoseconds.
namespace bw::delays {

inline constexpr Delays kLs00{9'000, 10'000};
inline constexpr Delays kLs02{10'000, 10'000};
inline constexpr Delays kLs04{9'000, 10'000};
inline constexpr Delays kLs08{8'000, 10'000};
inline constexpr Delays kLs20{9'000, 10'000};
inline constexpr Delays kLs30{8'000, 13'000};
inline constexpr Delays kLs32{14'000, 14'000};
inline constexpr Delays kLs86{12'000, 10'000};  // other input low
inline constexpr Delays kLs74{13'000, 25'000};  // CLK, PRE_N, CLR_N → Q, Q_N
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
inline constexpr Delays kLs164ClockToQ{17'000, 21'000};
inline constexpr Delays kLs164Clear{24'000, 24'000};  // only fall is used

}  // namespace bw::delays
