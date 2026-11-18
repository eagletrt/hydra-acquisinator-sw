#include "acquisinator/acquisinator-proto-interface.hpp"

void acquisinator_proto_interface_serialize_from_id(
    CanAcquisinatorMessageFrameId id, acquisinator::Pack *pack,
    union CanAcquisinatorMessage *messages) {
  switch (id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW: {
    acquisinator::acquisinator_acquisinator_raw *proto_msg =
        pack->add_acquisinator_raw();
    proto_msg->set_debug_adc_raw0_u(
        messages->acquisinator_raw.debug_adc_raw0_u);
    proto_msg->set_debug_adc_raw1_u(
        messages->acquisinator_raw.debug_adc_raw1_u);
    proto_msg->set_debug_adc_raw2_u(
        messages->acquisinator_raw.debug_adc_raw2_u);
    proto_msg->set_debug_seq_u(messages->acquisinator_raw.debug_seq_u);

#ifdef LIBCAN_TIMESTAMP
    proto_msg->set_inner_timestamp(messages->acquisinator_raw.timestamp);
#endif // LIBCAN_TIMESTAMP
    break;
  }
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN: {
    acquisinator::acquisinator_acquisinator_lockin *proto_msg =
        pack->add_acquisinator_lockin();
    proto_msg->set_lockin_inphase(messages->acquisinator_lockin.lockin_inphase);
    proto_msg->set_lockin_quadrature(
        messages->acquisinator_lockin.lockin_quadrature);

#ifdef LIBCAN_TIMESTAMP
    proto_msg->set_inner_timestamp(messages->acquisinator_lockin.timestamp);
#endif // LIBCAN_TIMESTAMP
    break;
  }
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS: {
    acquisinator::acquisinator_acquisinator_lockin_status *proto_msg =
        pack->add_acquisinator_lockin_status();
    proto_msg->set_lockin_dc_u(
        messages->acquisinator_lockin_status.lockin_dc_u);
    proto_msg->set_lockin_min_u(
        messages->acquisinator_lockin_status.lockin_min_u);
    proto_msg->set_lockin_max_u(
        messages->acquisinator_lockin_status.lockin_max_u);
    proto_msg->set_lockin_spi_error_b(
        messages->acquisinator_lockin_status.lockin_spi_error_b);
    proto_msg->set_lockin_seq_u(
        messages->acquisinator_lockin_status.lockin_seq_u);

#ifdef LIBCAN_TIMESTAMP
    proto_msg->set_inner_timestamp(
        messages->acquisinator_lockin_status.timestamp);
#endif // LIBCAN_TIMESTAMP
    break;
  }

  default: {
    return;
  }
  }
}