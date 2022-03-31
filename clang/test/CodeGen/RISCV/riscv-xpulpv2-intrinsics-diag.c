// RUN: %clang_cc1 %s -triple=riscv32 -verify -S -o -

#include <stdint.h>

void test_builtin_pulp_diag(int32_t *data) {
  // clang-format off
  (void) __builtin_pulp_CoreId(); // expected-error {{'__builtin_pulp_CoreId' needs target feature xpulpv}}
  (void) __builtin_pulp_CoreCount(); // expected-error {{'__builtin_pulp_CoreCount' needs target feature xpulpv}}
  (void) __builtin_pulp_ClusterId(); // expected-error {{'__builtin_pulp_ClusterId' needs target feature xpulpv}}
  (void) __builtin_pulp_IsFc(); // expected-error {{'__builtin_pulp_IsFc' needs target feature xpulpv}}
  (void) __builtin_pulp_HasFc(); // expected-error {{'__builtin_pulp_HasFc' needs target feature xpulpv}}
  (void) __builtin_pulp_mac(1, 2, 3); // expected-error {{'__builtin_pulp_mac' needs target feature xpulpv}}
  (void) __builtin_pulp_machhs(-(2u << 16), 4u << 16, 3); // expected-error {{'__builtin_pulp_machhs' needs target feature xpulpv}}
  (void) __builtin_pulp_machhu(2u << 16, 4u << 16, 3); // expected-error {{'__builtin_pulp_machhu' needs target feature xpulpv}}
  (void) __builtin_pulp_macs(2, -2, -3); // expected-error {{'__builtin_pulp_macs' needs target feature xpulpv}}
  (void) __builtin_pulp_macu(2, 2, 3); // expected-error {{'__builtin_pulp_macu' needs target feature xpulpv}}
  (void) __builtin_pulp_msu(1, 2, 10); // expected-error {{'__builtin_pulp_msu' needs target feature xpulpv}}
  (void) __builtin_pulp_bset(0x0, ((1u << 4) - 1) << 4); // expected-error {{'__builtin_pulp_bset' needs target feature xpulpv}}
  (void) __builtin_pulp_bset_r(0x0, 132u); // expected-error {{'__builtin_pulp_bset_r' needs target feature xpulpv}}
  (void) __builtin_pulp_clb(0x0F000000); // expected-error {{'__builtin_pulp_clb' needs target feature xpulpv}}
  (void) __builtin_pulp_cnt(0xFF0F); // expected-error {{'__builtin_pulp_cnt' needs target feature xpulpv}}
  (void) __builtin_pulp_ff1(0xF0); // expected-error {{'__builtin_pulp_ff1' needs target feature xpulpv}}
  (void) __builtin_pulp_fl1(31); // expected-error {{'__builtin_pulp_fl1' needs target feature xpulpv}}
  (void) __builtin_pulp_parity(5); // expected-error {{'__builtin_pulp_parity' needs target feature xpulpv}}
  (void) __builtin_pulp_rotr(0xFF0, 1); // expected-error {{'__builtin_pulp_rotr' needs target feature xpulpv}}
  (void) __builtin_pulp_abs(-2); // expected-error {{'__builtin_pulp_abs' needs target feature xpulpv}}
  (void) __builtin_pulp_addN(-10, 2, 1); // expected-error {{'__builtin_pulp_addN' needs target feature xpulpv}}
  (void) __builtin_pulp_addN_r(-10, 2, 1); // expected-error {{'__builtin_pulp_addN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_addRN(-10, 2, 2u, 2u); // expected-error {{'__builtin_pulp_addRN' needs target feature xpulpv}}
  (void) __builtin_pulp_addRN_r(-10, 2, 1); // expected-error {{'__builtin_pulp_addRN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_adduN(11, 2, 1); // expected-error {{'__builtin_pulp_adduN' needs target feature xpulpv}}
  (void) __builtin_pulp_adduN_r(11, 2, 1); // expected-error {{'__builtin_pulp_adduN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_adduRN(11, 2, 2u, 2u); // expected-error {{'__builtin_pulp_adduRN' needs target feature xpulpv}}
  (void) __builtin_pulp_adduRN_r(11, 2, 1); // expected-error {{'__builtin_pulp_adduRN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_clip(-10, -4, 15); // expected-error {{'__builtin_pulp_clip' needs target feature xpulpv}}
  (void) __builtin_pulp_clip_r(-10, 4); // expected-error {{'__builtin_pulp_clip_r' needs target feature xpulpv}}
  (void) __builtin_pulp_clipu(20, 0, 15); // expected-error {{'__builtin_pulp_clipu' needs target feature xpulpv}}
  (void) __builtin_pulp_clipu_r(10, 2); // expected-error {{'__builtin_pulp_clipu_r' needs target feature xpulpv}}
  (void) __builtin_pulp_maxsi(-1, 2); // expected-error {{'__builtin_pulp_maxsi' needs target feature xpulpv}}
  (void) __builtin_pulp_machhsN(-(7u << 16), 2u << 16, -2, 2); // expected-error {{'__builtin_pulp_machhsN' needs target feature xpulpv}}
  (void) __builtin_pulp_machhsRN(-(7u << 16), 2u << 16, -2, 2, 1); // expected-error {{'__builtin_pulp_machhsRN' needs target feature xpulpv}}
  (void) __builtin_pulp_machhuN(7u << 16, 2u << 16, -2, 2); // expected-error {{'__builtin_pulp_machhuN' needs target feature xpulpv}}
  (void) __builtin_pulp_machhuRN(7u << 16, 2u << 16, -2, 2, 1); // expected-error {{'__builtin_pulp_machhuRN' needs target feature xpulpv}}
  (void) __builtin_pulp_macsN(-7, 2, -2, 2); // expected-error {{'__builtin_pulp_macsN' needs target feature xpulpv}}
  (void) __builtin_pulp_macsRN(-7, 2, -2, 1, 1); // expected-error {{'__builtin_pulp_macsRN' needs target feature xpulpv}}
  (void) __builtin_pulp_macuN(7, 2, 2, 1); // expected-error {{'__builtin_pulp_macuN' needs target feature xpulpv}}
  (void) __builtin_pulp_macuRN(7, 2, 2, 1, 1); // expected-error {{'__builtin_pulp_macuRN' needs target feature xpulpv}}
  (void) __builtin_pulp_maxusi(1, 3); // expected-error {{'__builtin_pulp_maxusi' needs target feature xpulpv}}
  (void) __builtin_pulp_minsi(-1, 2); // expected-error {{'__builtin_pulp_minsi' needs target feature xpulpv}}
  (void) __builtin_pulp_minusi(1, 3); // expected-error {{'__builtin_pulp_minusi' needs target feature xpulpv}}
  (void) __builtin_pulp_mulhhs(-(8u << 16), 2u << 16); // expected-error {{'__builtin_pulp_mulhhs' needs target feature xpulpv}}
  (void) __builtin_pulp_mulhhsN(-(8u << 16), 2u << 16, 1); // expected-error {{'__builtin_pulp_mulhhsN' needs target feature xpulpv}}
  (void) __builtin_pulp_mulhhsRN(-(8u << 16), 2u << 16, 2, 1); // expected-error {{'__builtin_pulp_mulhhsRN' needs target feature xpulpv}}
  (void) __builtin_pulp_mulhhu(8u << 16, 2u << 16); // expected-error {{'__builtin_pulp_mulhhu' needs target feature xpulpv}}
  (void) __builtin_pulp_mulhhuN(8u << 16, 2u << 16, 1); // expected-error {{'__builtin_pulp_mulhhuN' needs target feature xpulpv}}
  (void) __builtin_pulp_mulhhuRN(8u << 16, 2u << 16, 2, 1); // expected-error {{'__builtin_pulp_mulhhuRN' needs target feature xpulpv}}
  (void) __builtin_pulp_muls(-7, 2); // expected-error {{'__builtin_pulp_muls' needs target feature xpulpv}}
  (void) __builtin_pulp_mulsN(-7, 2, 1); // expected-error {{'__builtin_pulp_mulsN' needs target feature xpulpv}}
  (void) __builtin_pulp_mulsRN(-7, 2, 2, 1); // expected-error {{'__builtin_pulp_mulsRN' needs target feature xpulpv}}
  (void) __builtin_pulp_mulu(7, 2); // expected-error {{'__builtin_pulp_mulu' needs target feature xpulpv}}
  (void) __builtin_pulp_muluN(7, 2, 1); // expected-error {{'__builtin_pulp_muluN' needs target feature xpulpv}}
  (void) __builtin_pulp_muluRN(7, 2, 2, 1); // expected-error {{'__builtin_pulp_muluRN' needs target feature xpulpv}}
  (void) __builtin_pulp_subN(-7, 2, 1); // expected-error {{'__builtin_pulp_subN' needs target feature xpulpv}}
  (void) __builtin_pulp_subN_r(7, 2, 1); // expected-error {{'__builtin_pulp_subN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_subRN(-7, 2, 2, 1); // expected-error {{'__builtin_pulp_subRN' needs target feature xpulpv}}
  (void) __builtin_pulp_subRN_r(-7, 2, 1); // expected-error {{'__builtin_pulp_subRN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_subuN(7, 2, 1); // expected-error {{'__builtin_pulp_subuN' needs target feature xpulpv}}
  (void) __builtin_pulp_subuN_r(7, 2, 1); // expected-error {{'__builtin_pulp_subuN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_subuRN(7, 2, 2, 1); // expected-error {{'__builtin_pulp_subuRN' needs target feature xpulpv}}
  (void) __builtin_pulp_subuRN_r(7, 2, 1); // expected-error {{'__builtin_pulp_subuRN_r' needs target feature xpulpv}}
  (void) __builtin_pulp_bclr(0x7FFFFFFF, 0xFFFF1FFF); // expected-error {{'__builtin_pulp_bclr' needs target feature xpulpv}}
  (void) __builtin_pulp_bclr_r(0xFFF, (3u << 5) | 4u); // expected-error {{'__builtin_pulp_bclr_r' needs target feature xpulpv}}
  (void) __builtin_pulp_bextract(-0x0000FE00, 0x8, 0x8); // expected-error {{'__builtin_pulp_bextract' needs target feature xpulpv}}
  (void) __builtin_pulp_bextract_r(-0x0000FE00, 0x88); // expected-error {{'__builtin_pulp_bextract_r' needs target feature xpulpv}}
  (void) __builtin_pulp_bextractu(0x0000FE00, 0x8, 0x8); // expected-error {{'__builtin_pulp_bextractu' needs target feature xpulpv}}
  (void) __builtin_pulp_bextractu_r(0x0000FE00, 0x88); // expected-error {{'__builtin_pulp_bextractu_r' needs target feature xpulpv}}
  (void) __builtin_pulp_binsert(0x0, 7, 1, 3, 0); // expected-error {{'__builtin_pulp_binsert' needs target feature xpulpv}}
  (void) __builtin_pulp_binsert_r(0x0, 7, 1); // expected-error {{'__builtin_pulp_binsert_r' needs target feature xpulpv}}
  (void) __builtin_pulp_OffsetedRead(data, 4); // expected-error {{'__builtin_pulp_OffsetedRead' needs target feature xpulpv}}
  (void) __builtin_pulp_OffsetedReadHalf((int16_t *) data, 4); // expected-error {{'__builtin_pulp_OffsetedReadHalf' needs target feature xpulpv}}
  (void) __builtin_pulp_OffsetedReadByte((char *) data, 4); // expected-error {{'__builtin_pulp_OffsetedReadByte' needs target feature xpulpv}}
  __builtin_pulp_OffsetedWrite(1, data, 4); // expected-error {{'__builtin_pulp_OffsetedWrite' needs target feature xpulpv}}
  __builtin_pulp_OffsetedWriteHalf(1, (int16_t *) data, 4); // expected-error {{'__builtin_pulp_OffsetedWriteHalf' needs target feature xpulpv}}
  __builtin_pulp_OffsetedWriteByte(1, (char *) data, 4); // expected-error {{'__builtin_pulp_OffsetedWriteByte' needs target feature xpulpv}}
  // clang-format on
}