#include "acquisinator/acquisinator-proto-interface.hpp"

void acquisinator_proto_interface_serialize_from_id(
    CanAcquisinatorMessageFrameId id, acquisinator::Pack *pack,
    union CanAcquisinatorMessage *messages) {
  switch (id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW: {
    acquisinator::acquisinator_acquisinator_raw *proto_msg =
        pack->add_acquisinator_raw();
    proto_msg->set_debug_adc_raw_u(messages->acquisinator_raw.debug_adc_raw_u);

#ifdef LIBCAN_TIMESTAMP
    proto_msg->set_inner_timestamp(messages->acquisinator_raw.timestamp);
#endif // LIBCAN_TIMESTAMP
    break;
  }

  default: {
    return;
  }
  }
}