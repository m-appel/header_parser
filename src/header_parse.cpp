/**
 * This program analyses a given MP4/H.264 video and outputs the frame and header information into a csv file. DEBUG and
 * INFO flags can be defined to get even more detailed output in stdout.
 */
#include "header_parse.h"
#include <iostream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <fstream>
#include <string>
#include <cmath>
#include "helper_functions.h"
#include "structs.h"
#include "defines.h"

using std::cout;
using std::cerr;
using std::endl;

std::vector<MP4Box> mp4_boxes;
std::vector<NALUnit> nal_units;
std::vector<SPS> spss;
std::vector<PPS> ppss;
std::vector<SliceHeader> slices;
std::ofstream csv_file;
uint32_t frame_num = 1;

void parseNALUnit(const uint8_t *addr, uint32_t &offset) {
#if defined(DEBUG) || defined(INFO)
  cout << "  NAL\n";
#endif
  uint8_t bit_offset = 0;
  NALUnit ret{};
  ret.location = addr + offset;
  // We manually add 4 because the size in the bytestream is excluding itself.
  ret.size = readUnsignedInt32(addr, offset, bit_offset, "size") + 4;
#ifdef INFO
  cout << "  size: " << ret.size << "\n";
#endif
  uint8_t forbidden_zero_bit = readBit(addr, offset, bit_offset, "forbidden_zero_bit");
  if (forbidden_zero_bit != 0) {
    cerr << "forbidden_zero_bit != 0\n";
  }
  ret.nal_ref_idc = readNBits(addr, offset, bit_offset, 2, "nal_ref_idc");
  ret.nal_unit_type = readNBits(addr, offset, bit_offset, 5, "nal_unit_type");
  nal_units.push_back(ret);
};
void parseSPS(const uint8_t *addr, uint32_t &offset) {
#if defined(DEBUG) || defined(INFO)
  cout << "    SPS\n";
#endif
  uint8_t bit_offset = 0;
  SPS ret{};
  ret.profile_idc = readByte(addr, offset, bit_offset, "profile_idc");
  ret.constraint_set0_flag = readBit(addr, offset, bit_offset, "constraint_set0_flag");
  ret.constraint_set1_flag = readBit(addr, offset, bit_offset, "constraint_set1_flag");
  ret.constraint_set2_flag = readBit(addr, offset, bit_offset, "constraint_set2_flag");
  ret.constraint_set3_flag = readBit(addr, offset, bit_offset, "constraint_set3_flag");
  ret.constraint_set4_flag = readBit(addr, offset, bit_offset, "constraint_set4_flag");
  ret.constraint_set5_flag = readBit(addr, offset, bit_offset, "constraint_set5_flag");
  uint8_t reserved_zero_2bits = readNBits(addr, offset, bit_offset, 2, "reserved_zero_2bits");
  if (reserved_zero_2bits != 0) {
    cerr << "sps: reserved_zero_2bits != 0\n";
  }

  ret.level_idc = readByte(addr, offset, bit_offset, "level_idc");
  ret.seq_parameter_set_id = decodeUnsignedExpGolomb(addr, offset, bit_offset, "seq_parameter_set_id");
  // See semantic note for this field as well as parset.c:117 in reference software.
  ret.chroma_format_idc = 1;
  if (ret.profile_idc == 100 || ret.profile_idc == 110 || ret.profile_idc == 122 || ret.profile_idc == 244
      || ret.profile_idc == 44 || ret.profile_idc == 83 || ret.profile_idc == 86 || ret.profile_idc == 118
      || ret.profile_idc == 128 || ret.profile_idc == 138 || ret.profile_idc == 139 || ret.profile_idc == 134) {
    ret.chroma_format_idc = decodeUnsignedExpGolomb(addr, offset, bit_offset, "chroma_format_idc");
    if (ret.chroma_format_idc == 3) {
      ret.separate_colour_plane_flag = readBit(addr, offset, bit_offset, "separate_colour_plane_flag");
    }
    ret.bit_depth_luma_minus8 = decodeUnsignedExpGolomb(addr, offset, bit_offset, "bit_depth_luma_minus8");
    ret.bit_depth_chroma_minus8 = decodeUnsignedExpGolomb(addr, offset, bit_offset, "bit_depth_chroma_minus8");
    ret.qpprime_y_zero_transform_bypass_flag = readBit(addr,
                                                       offset,
                                                       bit_offset,
                                                       "qpprime_y_zero_transform_bypass_flag");
    ret.seq_scaling_matrix_present_flag = readBit(addr, offset, bit_offset, "seq_scaling_matrix_present_flag");
    if (ret.seq_scaling_matrix_present_flag) {
      // TODO if (ret.seq_scaling_matrix_present_flag)...
      cerr << "WARNING: UNIMPLEMENTED CODE REACHED\n";
    }
  }
  ret.log2_max_frame_num_minus4 = decodeUnsignedExpGolomb(addr, offset, bit_offset, "log2_max_frame_num_minus4");
  ret.pic_order_cnt_type = decodeUnsignedExpGolomb(addr, offset, bit_offset, "pic_order_cnt_type");
  if (ret.pic_order_cnt_type == 0) {
    ret.log2_max_pic_order_cnt_lsb_minus4 = decodeUnsignedExpGolomb(addr,
                                                                    offset,
                                                                    bit_offset,
                                                                    "log2_max_pic_order_cnt_lsb_minus4");
  } else if (ret.pic_order_cnt_type == 1) {
    // TODO
    cerr << "WARNING: UNIMPLEMENTED CODE REACHED\n";
  }
  ret.max_num_ref_frames = decodeUnsignedExpGolomb(addr, offset, bit_offset, "max_num_ref_frames");
  ret.gaps_in_frame_num_value_allowed_flag = readBit(addr, offset, bit_offset, "gaps_in_frame_num_value_allowed_flag");
  ret.pic_width_in_mbs_minus1 = decodeUnsignedExpGolomb(addr, offset, bit_offset, "pic_width_in_mbs_minus1");
  ret.pic_height_in_map_units_minus1 = decodeUnsignedExpGolomb(addr,
                                                               offset,
                                                               bit_offset,
                                                               "pic_height_in_map_units_minus1");
  ret.frame_mbs_only_flag = readBit(addr, offset, bit_offset, "frame_mbs_only_flag");
  if (!ret.frame_mbs_only_flag) {
    ret.mb_adaptive_frame_field_flag = readBit(addr, offset, bit_offset, "mb_adaptive_frame_field_flag");
  }
  ret.direct_8x8_inference_flag = readBit(addr, offset, bit_offset, "direct_8x8_inference_flag");
  ret.frame_cropping_flag = readBit(addr, offset, bit_offset, "frame_cropping_flag");
  if (ret.frame_cropping_flag) {
    ret.frame_crop_left_offset = decodeUnsignedExpGolomb(addr, offset, bit_offset, "frame_crop_left_offset");
    ret.frame_crop_right_offset = decodeUnsignedExpGolomb(addr, offset, bit_offset, "frame_crop_right_offset");
    ret.frame_crop_top_offset = decodeUnsignedExpGolomb(addr, offset, bit_offset, "frame_crop_top_offset");
    ret.frame_crop_bottom_offset = decodeUnsignedExpGolomb(addr, offset, bit_offset, "frame_crop_bottom_offset");
  }
  ret.vui_parameters_present_flag = readBit(addr, offset, bit_offset, "vui_parameters_present_flag");
  // TODO if (ret.vui_parameters_present_flag) {
  spss.push_back(ret);
}

void parsePPS(const uint8_t *addr, uint32_t &offset) {
#if defined(DEBUG) || defined(INFO)
  cout << "    PPS\n";
#endif
  uint8_t bit_offset = 0;
  PPS ret{};
  ret.pic_parameter_set_id = decodeUnsignedExpGolomb(addr, offset, bit_offset, "pic_parameter_set_id");
  ret.seq_parameter_set_id = decodeUnsignedExpGolomb(addr, offset, bit_offset, "seq_parameter_set_id");
  ret.entropy_coding_mode_flag = readBit(addr, offset, bit_offset, "entropy_coding_mode_flag");
  ret.bottom_field_pic_order_in_frame_present_flag = readBit(addr,
                                                             offset,
                                                             bit_offset,
                                                             "bottom_field_pic_order_in_frame_present_flag");
  ret.num_slice_groups_minus1 = decodeUnsignedExpGolomb(addr, offset, bit_offset, "num_slice_groups_minus1");
  if (ret.num_slice_groups_minus1 > 0) {
    ret.slice_group_map_type = decodeUnsignedExpGolomb(addr, offset, bit_offset, "slice_group_map_type");
    if (ret.slice_group_map_type == 0) {
      for (uint32_t iGroup = 0; iGroup <= ret.num_slice_groups_minus1; iGroup++) {
        ret.run_length_minus1.push_back(decodeUnsignedExpGolomb(addr, offset, bit_offset, "run_length_minus1"));
      }
    } else if (ret.slice_group_map_type == 2) {
      for (uint32_t iGroup = 0; iGroup <= ret.num_slice_groups_minus1; iGroup++) {
        ret.top_left.push_back(decodeUnsignedExpGolomb(addr, offset, bit_offset, "top_left"));
        ret.bottom_right.push_back(decodeUnsignedExpGolomb(addr, offset, bit_offset, "top_right"));
      }
    } else if (ret.slice_group_map_type == 3 || ret.slice_group_map_type == 4 || ret.slice_group_map_type == 5) {
      ret.slice_group_change_direction_flag = readBit(addr, offset, bit_offset, "slice_group_change_direction_flag");
      ret.slice_group_change_rate_minus1 = decodeUnsignedExpGolomb(addr,
                                                                   offset,
                                                                   bit_offset,
                                                                   "slice_group_change_rate_minus1");
    } else if (ret.slice_group_map_type == 6) {
      ret.pic_size_in_map_units_minus1 = decodeUnsignedExpGolomb(addr,
                                                                 offset,
                                                                 bit_offset,
                                                                 "pic_size_in_map_units_minus1");
      uint32_t length = ceil(log2(ret.num_slice_groups_minus1 + 1));
      for (uint32_t i = 0; i < ret.pic_size_in_map_units_minus1; i++) {
        ret.slice_group_id.push_back(readNBits(addr, offset, bit_offset, length, "slice_group_id"));
      }
    }
  }
  ret.num_ref_idx_l0_default_active_minus1 = decodeUnsignedExpGolomb(addr,
                                                                     offset,
                                                                     bit_offset,
                                                                     "num_ref_idx_l0_default_active_minus1");
  ret.num_ref_idx_l1_default_active_minus1 = decodeUnsignedExpGolomb(addr,
                                                                     offset,
                                                                     bit_offset,
                                                                     "num_ref_idx_l1_default_active_minus1");
  ret.weighted_pred_flag = readBit(addr, offset, bit_offset, "weighted_pred_flag");
  ret.weighted_bipred_idc = static_cast<uint8_t>(readNBits(addr, offset, bit_offset, 2, "weighted_bipred_idc"));
  ret.pic_init_qp_minus26 = decodeSignedExpGolomb(addr, offset, bit_offset, "pic_init_qp_minus26");
  ret.pic_init_qs_minus26 = decodeSignedExpGolomb(addr, offset, bit_offset, "pic_init_qs_minus26");
  ret.chroma_qp_index_offset = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_qp_index_offset");
  ret.deblocking_filter_control_present_flag = readBit(addr,
                                                       offset,
                                                       bit_offset,
                                                       "deblocking_filter_control_present_flag");
  ret.constrained_intra_pred_flag = readBit(addr, offset, bit_offset, "constrained_intra_pred_flag");
  ret.redundant_pic_cnt_present_flag = readBit(addr, offset, bit_offset, "redundant_pic_cnt_present_flag");
  // TODO if(more_rbsp_data())
  ppss.push_back(ret);
}

void parseSliceHeader(const uint8_t *addr, uint32_t &offset) {
#if defined(DEBUG) || defined(INFO)
  uint32_t offset_start = offset;
#endif
#ifdef DEBUG
  cout << "    Slice\n";
#endif
  uint8_t bit_offset = 0;
  SliceHeader ret{};
  SPS &curr_sps = spss.back();
  PPS &curr_pps = ppss.back();
  NALUnit &curr_nal_unit = nal_units.back();
  // Don't know if this works, but we are not really interested in the value anyways.
  uint32_t mb_address = decodeUnsignedExpGolomb(addr, offset, bit_offset, "first_mb_in_slice");
  uint8_t *ptr = nullptr;
  ret.first_mb_in_slice = ptr + mb_address;
  ret.slice_type = decodeUnsignedExpGolomb(addr, offset, bit_offset, "slice_type");
  std::string slice_type_string = getSliceTypeString(ret.slice_type);
#ifdef INFO
  cout << "    " << slice_type_string << " Slice\n";
#endif
  csv_file << slice_type_string << "," << frame_num++ << "," << curr_nal_unit.size << "\n";
  ret.pic_parameter_set_id = decodeUnsignedExpGolomb(addr, offset, bit_offset, "pic_parameter_set_id");
  if (curr_sps.separate_colour_plane_flag) {
    ret.colour_plane_id = static_cast<uint8_t>(readNBits(addr, offset, bit_offset, 2, "colour_plane_id"));
  }
  ret.frame_num = readNBits(addr, offset, bit_offset, curr_sps.log2_max_frame_num_minus4 + 4, "frame_num");
  if (!curr_sps.frame_mbs_only_flag) {
    ret.field_pic_flag = readBit(addr, offset, bit_offset, "field_pic_flag");
    if (ret.field_pic_flag) {
      ret.bottom_field_flag = readBit(addr, offset, bit_offset, "bottom_field_flag");
    }
  }
  // IdrPicFlag
  if (curr_nal_unit.nal_unit_type == 5) {
    ret.idr_pic_id = decodeUnsignedExpGolomb(addr, offset, bit_offset, "idr_pic_id");
  }
  if (curr_sps.pic_order_cnt_type == 0) {
    ret.pic_order_cnt_lsb = readNBits(addr,
                                      offset,
                                      bit_offset,
                                      curr_sps.log2_max_pic_order_cnt_lsb_minus4 + 4,
                                      "pic_order_cnt_lsb");
    if (curr_pps.bottom_field_pic_order_in_frame_present_flag && !ret.field_pic_flag) {
      ret.delta_pic_order_cnt_bottom = decodeSignedExpGolomb(addr, offset, bit_offset, "delta_pic_order_cnt_bottom");
    }
  }
  if (curr_sps.pic_order_cnt_type == 1 && !curr_sps.delta_pic_order_always_zero_flag) {
    ret.delta_pic_order_cnt_0 = decodeSignedExpGolomb(addr, offset, bit_offset, "delta_pic_order_cnt_0");
    if (curr_pps.bottom_field_pic_order_in_frame_present_flag && !ret.field_pic_flag) {
      ret.delta_pic_order_cnt_1 = decodeSignedExpGolomb(addr, offset, bit_offset, "delta_pic_order_cnt_1");
    }
  }
  if (curr_pps.redundant_pic_cnt_present_flag) {
    ret.redundant_pic_cnt = decodeUnsignedExpGolomb(addr, offset, bit_offset, "redundant_pic_cnt");
  }
  if (slice_type_string == "B") {
    ret.direct_spatial_mv_pred_flag = readBit(addr, offset, bit_offset, "direct_spatial_mv_pred_flag");
  }
  if (slice_type_string == "P" || slice_type_string == "SP" || slice_type_string == "B") {
    ret.num_ref_idx_active_override_flag = readBit(addr, offset, bit_offset, "num_ref_idx_active_override_flag");
    if (ret.num_ref_idx_active_override_flag) {
      ret.num_ref_idx_l0_active_minus1 = decodeUnsignedExpGolomb(addr,
                                                                 offset,
                                                                 bit_offset,
                                                                 "num_ref_idx_l0_active_minus1");
      if (slice_type_string == "B") {
        ret.num_ref_idx_l1_active_minus1 = decodeUnsignedExpGolomb(addr,
                                                                   offset,
                                                                   bit_offset,
                                                                   "num_ref_idx_l1_active_minus1");
      }
    }
  }
  if (curr_nal_unit.nal_unit_type == 20 || curr_nal_unit.nal_unit_type == 21) {
    // TODO ref_pic_list_mvc_modification()
    cerr << "WARNING: UNIMPLEMENTED CODE REACHED\n";
  } else {
    // ref_pic_list_modification()
    if (ret.slice_type % 5 != 2 && ret.slice_type % 5 != 4) {
      ret.ref_pic_list_modification_flag_l0 = readBit(addr, offset, bit_offset, "ref_pic_list_modification_flag_l0");
      if (ret.ref_pic_list_modification_flag_l0) {
        do {
          ret.modification_of_pic_nums_idc = decodeUnsignedExpGolomb(addr,
                                                                     offset,
                                                                     bit_offset,
                                                                     "modification_of_pic_nums_idc");
          if (ret.modification_of_pic_nums_idc == 0 || ret.modification_of_pic_nums_idc == 1) {
            ret.abs_diff_pic_num_minus1 = decodeUnsignedExpGolomb(addr, offset, bit_offset, "abs_diff_pic_num_minus1");
          } else if (ret.modification_of_pic_nums_idc == 2) {
            ret.long_term_pic_num = decodeUnsignedExpGolomb(addr, offset, bit_offset, "long_term_pic_num");
          }
        } while (ret.modification_of_pic_nums_idc != 3);
      }
    }
    if (ret.slice_type % 5 == 1) {
      ret.ref_pic_list_modification_flag_l1 = readBit(addr, offset, bit_offset, "ref_pic_list_modification_flag_l1");
      if (ret.ref_pic_list_modification_flag_l1) {
        do {
          ret.modification_of_pic_nums_idc = decodeUnsignedExpGolomb(addr,
                                                                     offset,
                                                                     bit_offset,
                                                                     "modification_of_pic_nums_idc");
          if (ret.modification_of_pic_nums_idc == 0 || ret.modification_of_pic_nums_idc == 1) {
            ret.abs_diff_pic_num_minus1 = decodeUnsignedExpGolomb(addr, offset, bit_offset, "abs_diff_pic_num_minus1");
          } else if (ret.modification_of_pic_nums_idc == 2) {
            ret.long_term_pic_num = decodeUnsignedExpGolomb(addr, offset, bit_offset, "long_term_pic_num");
          }
        } while (ret.modification_of_pic_nums_idc != 3);
      }
    }
  }
  if ((curr_pps.weighted_pred_flag && (slice_type_string == "P" || slice_type_string == "SP"))
      || (curr_pps.weighted_bipred_idc == 1 && slice_type_string == "B")) {
    // pred_weight_table()
    ret.luma_log2_weight_denom = decodeUnsignedExpGolomb(addr, offset, bit_offset, "luma_log2_weight_denom");
    if (getChromaArrayType(curr_sps) != 0) {
      ret.chroma_log2_weight_denom = decodeUnsignedExpGolomb(addr, offset, bit_offset, "chroma_log2_weight_denom");
    }
    for (uint32_t i = 0; i <= ret.num_ref_idx_l0_active_minus1; i++) {
      ret.luma_weight_l0_flag = readBit(addr, offset, bit_offset, "luma_weight_l0_flag");
      if (ret.luma_weight_l0_flag) {
        ret.luma_weight_l0.push_back(decodeSignedExpGolomb(addr, offset, bit_offset, "luma_weight_l0"));
        ret.luma_offset_l0.push_back(decodeSignedExpGolomb(addr, offset, bit_offset, "luma_offset_l0"));
      }
      if (getChromaArrayType(curr_sps) != 0) {
        ret.chroma_weight_l0_flag = readBit(addr, offset, bit_offset, "chroma_weight_l0_flag");
        if (ret.chroma_weight_l0_flag) {
          std::pair<int32_t, int32_t> chroma_weight_pair;
          std::pair<int32_t, int32_t> chroma_offset_pair;
          chroma_weight_pair.first = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_weight_l0");
          chroma_offset_pair.first = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_offset_l0");
          chroma_weight_pair.second = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_weight_l0");
          chroma_offset_pair.second = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_offset_l0");
          ret.chroma_weight_l0.push_back(chroma_weight_pair);
          ret.chroma_offset_l0.push_back(chroma_offset_pair);
        }
      }
    }
    if (ret.slice_type % 5 == 1) {
      for (uint32_t i = 0; i <= ret.num_ref_idx_l1_active_minus1; i++) {
        ret.luma_weight_l1_flag = readBit(addr, offset, bit_offset, "luma_weight_l1_flag");
        if (ret.luma_weight_l1_flag) {
          ret.luma_weight_l1.push_back(decodeSignedExpGolomb(addr, offset, bit_offset, "luma_weight_l1"));
          ret.luma_offset_l1.push_back(decodeSignedExpGolomb(addr, offset, bit_offset, "luma_offset_l1"));
        }
        if (getChromaArrayType(curr_sps) != 0) {
          ret.chroma_weight_l1_flag = readBit(addr, offset, bit_offset, "");
          if (ret.chroma_weight_l1_flag) {
            std::pair<int32_t, int32_t> chroma_weight_pair;
            std::pair<int32_t, int32_t> chroma_offset_pair;
            chroma_weight_pair.first = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_weight_l1");
            chroma_offset_pair.first = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_offset_l1");
            chroma_weight_pair.second = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_weight_l1");
            chroma_offset_pair.second = decodeSignedExpGolomb(addr, offset, bit_offset, "chroma_offset_l1");
            ret.chroma_weight_l1.push_back(chroma_weight_pair);
            ret.chroma_offset_l1.push_back(chroma_offset_pair);
          }
        }
      }
    }
  }
  if (curr_nal_unit.nal_ref_idc != 0) {
    // dec_ref_pic_marking()
    // IdrPicFlag
    if (curr_nal_unit.nal_unit_type == 5) {
      ret.no_output_of_prior_pics_flag = readBit(addr, offset, bit_offset, "no_output_of_prior_pics_flag");
      ret.long_term_reference_flag = readBit(addr, offset, bit_offset, "long_term_reference_flag");
    } else {
      ret.adaptive_ref_pic_marking_mode_flag = readBit(addr, offset, bit_offset, "adaptive_ref_pic_marking_mode_flag");
      if (ret.adaptive_ref_pic_marking_mode_flag) {
        do {
          ret.memory_management_control_operation = decodeUnsignedExpGolomb(addr,
                                                                            offset,
                                                                            bit_offset,
                                                                            "memory_management_control_operation");
          if (ret.memory_management_control_operation == 1 || ret.memory_management_control_operation == 3) {
            ret.difference_of_pic_nums_minus1 = decodeUnsignedExpGolomb(addr,
                                                                        offset,
                                                                        bit_offset,
                                                                        "difference_of_pic_nums_minus1");
          }
          if (ret.memory_management_control_operation == 2) {
            ret.long_term_pic_num = decodeUnsignedExpGolomb(addr, offset, bit_offset, "long_term_pic_num");
          }
          if (ret.memory_management_control_operation == 3 || ret.memory_management_control_operation == 6) {
            ret.long_term_frame_idx = decodeUnsignedExpGolomb(addr, offset, bit_offset, "long_term_frame_idx");
          }
          if (ret.memory_management_control_operation == 4) {
            ret.max_long_term_frame_idx_plus1 = decodeUnsignedExpGolomb(addr,
                                                                        offset,
                                                                        bit_offset,
                                                                        "max_long_term_frame_idx_plus1");
          }
        } while (ret.memory_management_control_operation != 0);
      }
    }
  }
  if (curr_pps.entropy_coding_mode_flag && slice_type_string != "I" && slice_type_string != "SI") {
    ret.cabac_init_idc = decodeUnsignedExpGolomb(addr, offset, bit_offset, "cabac_init_idc");
  }
  ret.slice_qp_delta = decodeSignedExpGolomb(addr, offset, bit_offset, "slice_qp_delta");
  if (slice_type_string == "SP" || slice_type_string == "SI") {
    if (slice_type_string == "SP") {
      ret.sp_for_switch_flag = readBit(addr, offset, bit_offset, "sp_for_switch_flag");
    }
    ret.slice_qs_delta = decodeSignedExpGolomb(addr, offset, bit_offset, "slice_qs_delta");
  }
  if (curr_pps.deblocking_filter_control_present_flag) {
    ret.disable_deblocking_filter_idc = decodeUnsignedExpGolomb(addr,
                                                                offset,
                                                                bit_offset,
                                                                "disable_deblocking_filter_idc");
    if (ret.disable_deblocking_filter_idc != 1) {
      ret.slice_alpha_c0_offset_div2 = decodeSignedExpGolomb(addr, offset, bit_offset, "slice_alpha_c0_offset_div2");
      ret.slice_beta_offset_div2 = decodeSignedExpGolomb(addr, offset, bit_offset, "slice_beta_offset_div2");
    }
  }
  if (curr_pps.num_slice_groups_minus1 > 0 && curr_pps.slice_group_map_type >= 3
      && curr_pps.slice_group_map_type <= 5) {
    double pic_size_in_map_units = curr_pps.pic_size_in_map_units_minus1 + 1;
    double slice_group_change_rate = curr_pps.slice_group_change_rate_minus1 + 1;
    uint32_t length = ceil(log2(pic_size_in_map_units / slice_group_change_rate + 1));
    ret.slice_group_change_cycle = readNBits(addr, offset, bit_offset, length, "slice_group_change_cycle");
  }
  slices.push_back(ret);
#if defined(DEBUG) || defined(INFO)
  cout << "    Slice header length: " << offset - offset_start << " bytes " << +bit_offset << " bits" << "\n";
  printf("    Slice data @ %X+%u\n", offset, bit_offset);
#endif
}

int32_t parseMP4Box(const uint8_t *addr, uint32_t &offset) {
#if defined(DEBUG) || defined(INFO)
  cout << "MP4\n";
#endif
  uint8_t bit_offset = 0;
  MP4Box ret{};
  ret.location = addr + offset;
  ret.size = readUnsignedInt32(addr, offset, bit_offset, "size");
#ifdef DEBUG
  // Need to print this manually, because we would print the integer representation, which is not useful.
  std::cout << POSITION << "name: ";
#endif
  uint32_t box_type = readUnsignedInt32(addr, offset, bit_offset);
  ret.name = getNameString(box_type);
#if defined(DEBUG)
  std::cout << ret.name << "\n";
#endif
#ifdef INFO
  cout << ret.name << " size: " << ret.size << "\n";
#endif
  if (ret.size <= 1) {
    //TODO
    cerr << "largesize or eof\n";
    return -1;
  }
  if (ret.name != "mdat") {
    csv_file << "H,0," << ret.size << "\n";
    // Skip the actual contents of this box
    offset += ret.size - 8;
  } else {
    // The output in the csv for the NAL units only includes their own size, so we need to add the size of the mdat
    // header manually to get a byte count w/o gaps.
    csv_file << "H,0,8\n";
  }
  mp4_boxes.push_back(ret);
  return 0;
}

int main(int32_t argc, char **argv) {
  if (argc < 3) {
    cout << "usage: " << argv[0] << " <video> <csv-file>" << endl;
    return 1;
  }
  csv_file.open(argv[2], std::ofstream::trunc);
  if (csv_file.fail()) {
    cerr << "failed to open csv-file: " << strerror(errno) << "\n";
    return 1;
  }
  csv_file << "type,num,size\n";
  struct stat st{};
  if (stat(argv[1], &st) < 0) {
    cerr << "error while getting file size: " << strerror(errno) << "\n";
    return 1;
  }
  size_t file_size = st.st_size;
  int32_t file_fd = open(argv[1], O_RDONLY);
  if (file_fd < 0) {
    cerr << "could not open fd: " << strerror(errno) << "\n";
    return 1;
  }
  auto *file_mmap = static_cast<uint8_t *>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, file_fd, 0));
  if (file_mmap == MAP_FAILED) {
    close(file_fd);
    cerr << "could not mmap file: " << strerror(errno) << "\n";
    return 1;
  }
  close(file_fd);
  if (madvise(file_mmap, file_size, MADV_SEQUENTIAL) < 0) {
    cerr << "madvise: " << strerror(errno) << "\n";
  }

  uint32_t offset = 0;
  while (file_mmap + offset < file_mmap + file_size) {
    int32_t res = parseMP4Box(file_mmap, offset);
    if (res < 0) {
      cerr << "ERRR\n";
      break;
    }
    MP4Box &last_mp4 = mp4_boxes.back();
    if (last_mp4.name == "mdat") {
      const uint8_t *mdat_end = last_mp4.location + last_mp4.size;
      while (file_mmap + offset < mdat_end) {
        parseNALUnit(file_mmap, offset);
        NALUnit &last_nal = nal_units.back();
        // We need this to position the offset correctly, because we do not parse all parts of the NAL unit, e.g., the
        // macro blocks of a slice.
        uint32_t after_offset = offset + last_nal.size - 5;
        if (last_nal.nal_unit_type == 7) {
          csv_file << "H,0," << last_nal.size << "\n";
          parseSPS(file_mmap, offset);
#ifdef DEBUG
          cout << "Skipping " << after_offset - offset << " bytes of vui_parameters()\n";
#endif
        } else if (last_nal.nal_unit_type == 8) {
          csv_file << "H,0," << last_nal.size << "\n";
          parsePPS(file_mmap, offset);
        } else if (last_nal.nal_unit_type == 1 || last_nal.nal_unit_type == 5) {
          parseSliceHeader(file_mmap, offset);
        } else {
          csv_file << "H,0," << last_nal.size << "\n";
#if defined(DEBUG) || defined(INFO)
          cout << "    Other\n";
#endif
        }
        offset = after_offset;
      }
    }
  }

  csv_file.close();
  if (csv_file.fail()) {
    cerr << "csv-file close: " << strerror(errno) << "\n";
  }

  if (munmap(file_mmap, file_size) < 0) {
    cerr << "munmap: " << strerror(errno) << "\n";
  }
  return 0;
}