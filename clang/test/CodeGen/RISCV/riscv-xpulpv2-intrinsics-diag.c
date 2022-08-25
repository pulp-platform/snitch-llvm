// REQUIRES: riscv-registered-target
// RUN: %clang_cc1 %s -triple=riscv32 -verify -S -o -

#include <stdint.h>

void test_builtin_pulp_diag(int32_t *data) {
  // clang-format off
  (void) __builtin_pulp_CoreId(); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_CoreCount(); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_ClusterId(); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_IsFc(); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_HasFc(); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mac(1, 2, 3); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_machhs(-(2u << 16), 4u << 16, 3); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_machhu(2u << 16, 4u << 16, 3); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_macs(2, -2, -3); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_macu(2, 2, 3); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_msu(1, 2, 10); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bset(0x0, ((1u << 4) - 1) << 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bset_r(0x0, 132u); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_clb(0x0F000000); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_cnt(0xFF0F); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_ff1(0xF0); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_fl1(31); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_parity(5); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_rotr(0xFF0, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_abs(-2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_addN(-10, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_addN_r(-10, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_addRN(-10, 2, 2u, 2u); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_addRN_r(-10, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_adduN(11, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_adduN_r(11, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_adduRN(11, 2, 2u, 2u); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_adduRN_r(11, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_clip(-10, -4, 15); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_clip_r(-10, 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_clipu(20, 0, 15); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_clipu_r(10, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_maxsi(-1, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_machhsN(-(7u << 16), 2u << 16, -2, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_machhsRN(-(7u << 16), 2u << 16, -2, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_machhuN(7u << 16, 2u << 16, -2, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_machhuRN(7u << 16, 2u << 16, -2, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_macsN(-7, 2, -2, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_macsRN(-7, 2, -2, 1, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_macuN(7, 2, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_macuRN(7, 2, 2, 1, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_maxusi(1, 3); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_minsi(-1, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_minusi(1, 3); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulhhs(-(8u << 16), 2u << 16); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulhhsN(-(8u << 16), 2u << 16, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulhhsRN(-(8u << 16), 2u << 16, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulhhu(8u << 16, 2u << 16); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulhhuN(8u << 16, 2u << 16, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulhhuRN(8u << 16, 2u << 16, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_muls(-7, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulsN(-7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulsRN(-7, 2, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_mulu(7, 2); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_muluN(7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_muluRN(7, 2, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subN(-7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subN_r(7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subRN(-7, 2, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subRN_r(-7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subuN(7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subuN_r(7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subuRN(7, 2, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_subuRN_r(7, 2, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bclr(0x7FFFFFFF, 0xFFFF1FFF); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bclr_r(0xFFF, (3u << 5) | 4u); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bextract(-0x0000FE00, 0x8, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bextract_r(-0x0000FE00, 0x88); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bextractu(0x0000FE00, 0x8, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_bextractu_r(0x0000FE00, 0x88); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_binsert(0x0, 7, 1, 3, 0); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_binsert_r(0x0, 7, 1); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_OffsetedRead(data, 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_OffsetedReadHalf((int16_t *) data, 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_OffsetedReadByte((char *) data, 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_OffsetedWrite(1, data, 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_OffsetedWriteHalf(1, (int16_t *) data, 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_OffsetedWriteByte(1, (char *) data, 4); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_read_base_off(data, 0xF); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_write_base_off(0x1, data, 0xF); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_read_base_off_v(data, 0xF); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_write_base_off_v(0x1, data, 0xF); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_read_then_spr_bit_clr(0xF14, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_read_then_spr_bit_set(0xF14, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_read_then_spr_write(0xF14, 8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_spr_bit_clr(0xF14, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_spr_bit_set(0xF14, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_spr_read(0xF14); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_spr_read_vol(0xF14); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  __builtin_pulp_spr_write(0xF14, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  (void) __builtin_pulp_event_unit_read(data, 0x8); // expected-error {{builtin requires at least one of the following extensions support to be enabled : 'Xpulpv'}}
  // clang-format on
}