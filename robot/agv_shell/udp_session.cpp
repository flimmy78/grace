#include "udp_session.h"
#include "file_read_handler.h"
#include <iostream>
#include "log.h"

udp_session::udp_session(){

}

udp_session::~udp_session(){

}

void udp_session::on_recvdata(const std::string &data, const nsp::tcpip::endpoint &r_ep){
	nsp::proto::udp::unpackage upkt(data);
	switch (upkt.ack_type())
	{
	case EXECUTE_ACK::ACK_Normal:
		break;
	case EXECUTE_ACK::ACK_Version_Failed:
	case EXECUTE_ACK::ACK_Limits:
	case EXECUTE_ACK::ACK_Command_No:
	case EXECUTE_ACK::ACK_Addr_Not_Write:
	case EXECUTE_ACK::ACK_OverFlow_Memory:
	case EXECUTE_ACK::ACK_Content_Invaild:
	case EXECUTE_ACK::ACK_VCU_Busy:
	case EXECUTE_ACK::ACK_Target_Port_Faild:
	case EXECUTE_ACK::ACK_Target_Not_exist:
	case EXECUTE_ACK::ACK_Target_Not_Reponse:
	case EXECUTE_ACK::ACK_Data_Lose:
	case EXECUTE_ACK::ACK_Other_Problem:
		return dispatch_vcu_error_info(upkt.pkt_id(), (EXECUTE_ACK)upkt.ack_type());
	default:
		break;
	}
	switch (upkt.main_operate())
	{
	case FIRMWARE_MAIN_OPERATE_CODE_MODULE_CMD:
	case FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE:
	case FIRMWARE_CAN_COMMOND_START_DATA_FORWARD:
		dispatch_vcu_recv_info(upkt.main_operate(), upkt.pkt_id(), upkt.shared_for<nsp::proto::udp::recv_data>(), r_ep);
		break;
	case FIRMWARE_MAIN_OPERATE_CODE_GET_INFO:
		dispatch_vcu_get_info(upkt.pkt_id(), upkt.shared_for<nsp::proto::udp::recv_data>(), r_ep);
		break;
	case FIRMWARE_MAIN_OPERATE_CODE_RW_FIRMWARE:
		dispatch_vcu_rw_firmware(upkt.pkt_id(), upkt.sub_operate(), upkt.shared_for<nsp::proto::udp::recv_data>(), r_ep);
		break;
	case FIRMWARE_MAIN_OPERATE_CODE_KEEPALIVE:
		dispatch_vcu_keep_alive(upkt.pkt_id(), upkt.shared_for<nsp::proto::udp::recv_data>(), r_ep);
		break;
	default:
		break;
	}
}

void udp_session::dispatch_vcu_error_info(const uint16_t pkt_id, const EXECUTE_ACK type)
{
	lotrace("agv_shell")<< "receive error, pkt_id:"<< pkt_id << ", type:" << type;
	recv_vcu_data data;
	data.set_err(nsp::proto::kUnsuccessful);
	nsp::toolkit::singleton<net_manager>::instance()->exec(pkt_id, (char *)&data);
	return;
}

void udp_session::dispatch_vcu_recv_info(const uint16_t main_operate, const uint16_t pkt_id, 
	const std::shared_ptr<nsp::proto::udp::recv_data>& pkt, const nsp::tcpip::endpoint &r_ep){
	recv_vcu_data data;
	data.set_err(nsp::proto::kSuccessful);
	data.remote_ip_ = r_ep.ipv4();
	data.data_length_ = pkt->data_len_;
	data.data_context_ = pkt->data_;
	
	if (main_operate != FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE)
	{
		nsp::toolkit::singleton<net_manager>::instance()->exec(pkt_id, (char *)&data);
	}
	else
	{
		nsp::proto::udp::can_data_t can_data;
		int nLength = data.data_length_;
		can_data.build((unsigned char *)data.data_context_.data(), nLength);

		if (can_data.can_cmd == FIRMWARE_CAN_COMMOND_CHECKSUM_FAIL_ACK)
		{
			data.set_err(nsp::proto::kSecurityFatal);
			nsp::toolkit::singleton<net_manager>::instance()->exec_data_forward(FIRMWARE_CAN_COMMOND_DOWNLOAD_PACKET_ACK, (char *)&data);
		}
		else
		{
			nsp::toolkit::singleton<net_manager>::instance()->exec_data_forward(can_data.can_cmd, (char *)&data);
		}
	}
	return;
}

void udp_session::dispatch_vcu_get_info(const uint16_t pkt_id, const std::shared_ptr<nsp::proto::udp::recv_data>& pkt, const nsp::tcpip::endpoint &r_ep){
	recv_vcu_data data;
	data.set_err(nsp::proto::kSuccessful);
	data.remote_ip_ = r_ep.ipv4();
	data.data_length_ = pkt->data_len_;
	data.data_context_ = pkt->data_;
	nsp::toolkit::singleton<net_manager>::instance()->exec(pkt_id, (char *)&data);
	return;
}

void udp_session::dispatch_vcu_rw_firmware(const uint16_t pkt_id, const short int sub_operate,
	const std::shared_ptr<nsp::proto::udp::recv_data>& pkt, const nsp::tcpip::endpoint &r_ep){
	if (sub_operate == FIRMWARE_SUB_OPERATE_CODE_READ_BOOT_FILE || sub_operate == FIRMWARE_SUB_OPERATE_CODE_READ_MAIN_FILE 
		|| sub_operate == FIRMWARE_SUB_OPERATE_CODE_READ_USER_FILE || sub_operate == FIRMWARE_SUB_OPERATE_CODE_READ_NORAML_FILE){
		recv_bin_data data;
		data.set_err(nsp::proto::kSuccessful);
		memcpy(&data.offset_,  pkt->data_.c_str(), sizeof(data.offset_));
		memcpy(&data.data_length_,  pkt->data_.substr(4, 4).c_str(), sizeof(data.data_length_));
		data.data_context_ = pkt->data_.substr(8, pkt->data_.size() - 8);
		nsp::toolkit::singleton<net_manager>::instance()->exec(pkt_id, (char *)&data);
	}
	else{
		recv_vcu_data data;
		data.set_err(nsp::proto::kSuccessful);
		data.remote_ip_ = r_ep.ipv4();
		nsp::toolkit::singleton<net_manager>::instance()->exec(pkt_id, (char *)&data);
	}
	
	return;
}

void udp_session::dispatch_vcu_keep_alive(const uint16_t pkt_id, const std::shared_ptr<nsp::proto::udp::recv_data>& pkt, const nsp::tcpip::endpoint &r_ep){
	recv_vcu_data data;
	data.set_err(nsp::proto::kSuccessful);
	data.remote_ip_ = r_ep.ipv4();
	data.data_length_ = pkt->data_len_;
	data.data_context_ = pkt->data_;
    if(pkt->data_len_ > 0) {
        const unsigned char *stream_pos = (unsigned char*)pkt->data_.c_str();
        int t_len = pkt->data_len_;
        nsp::proto::proto_crt_t<uint32_t> element_count;
        stream_pos = element_count.build(stream_pos, t_len);
        lotrace("agv_shell")<<"  before element_count.value_:" << element_count.value_;
        data.data_context_ = std::to_string(element_count.value_);
    }
    nsp::toolkit::singleton<net_manager>::instance()->exec(pkt_id, (char *)&data);
	return;
}

int udp_session::post_vcu_type_request(const nsp::tcpip::endpoint& ep,
	const std::shared_ptr<motion::asio_block> &asio_ack){
	loinfo("agv_shell") << "udp:send query vcu type request,the target endpoint is " << ep.to_string();
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_GET_INFO, FIRMWARE_SUB_OPERATE_CODE_GET_VCU_TYPE, pkt_id);
	c_data.data_len_ = 0;
	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_cpu_type_requst(const nsp::tcpip::endpoint& ep, const std::shared_ptr<motion::asio_block> &asio_ack)
{
	loinfo("agv_shell") << "udp:send query cpu info request,the target endpoint is " << ep.to_string();
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_GET_INFO, FIRMWARE_SUB_OPERATE_CODE_GET_CPU_VERSION,pkt_id);
	c_data.data_len_ = 0;
	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]()->int{
		return psend(c_data,ep);
	},asio_ack);
}

int udp_session::post_vcu_version_request(const FIRMWARE_SOFTWARE_TYPE type, const nsp::tcpip::endpoint& ep, const int is_control,
	const std::shared_ptr<motion::asio_block> &asio_ack){
	loinfo("agv_shell") << "udp:send query vcu version info request,the target endpoint is " << ep.to_string();
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_GET_INFO, pkt_id);
	if (is_control == 1){
		switch (type)
		{
		case FIRMWARE_SOFTWARE_TYPE::BOOT_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_BOOT_VERSION);
			break;
		case FIRMWARE_SOFTWARE_TYPE::MAIN_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_MAIN_VERSION);
			break;
		case FIRMWARE_SOFTWARE_TYPE::USER_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_USER_VERSION);
			break;
		case FIRMWARE_SOFTWARE_TYPE::CAMERA_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_CAMERA_VERSION);
			break;
		default:
			break;
		}
		c_data.data_len_ = 0;
	}
	else if (is_control == 0){
		switch (type)
		{
		case FIRMWARE_SOFTWARE_TYPE::BOOT_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_UNCONTROL_GET_BOOT_VERSION);
			break;
		case FIRMWARE_SOFTWARE_TYPE::MAIN_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_UNCONTROL_GET_MAIN_VERSION);
			break;
		case FIRMWARE_SOFTWARE_TYPE::USER_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_UNCONTROL_GET_USER_VERSION);
			break;
		case FIRMWARE_SOFTWARE_TYPE::CAMERA_SOFTWARE:
			c_data.set_sub_operate(FIRMWARE_UNCONTROL_GET_CAMERA_VERSION);
			break;
		default:
			break;
		}
		c_data.data_len_ = 4;
		uint32_t size = nsp::toolkit::singleton<file_read_handler>::instance()->get_file_size();
		char length[4];
		memcpy(length,  &size, sizeof(size));
		c_data.data_.assign(length, sizeof(size));
	}
	
	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_vcu_reset_request(const nsp::tcpip::endpoint& ep, const uint32_t file_size,
	const std::shared_ptr<motion::asio_block> &asio_ack){
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_MODULE_CMD, FIRMWARE_SUB_OPERATE_CODE_RESET_VCU, pkt_id);
	c_data.data_len_ = sizeof(file_size);
	char buffer[4];
	memcpy(buffer, &file_size, sizeof(file_size));
	c_data.data_.assign(buffer, sizeof(buffer));
	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_vcu_write_bin_file(const FIRMWARE_SOFTWARE_TYPE type, const nsp::tcpip::endpoint& ep,
	const int block_offset, const std::string&file_block_data,
	const std::shared_ptr<motion::asio_block>& asio_ack){
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_RW_FIRMWARE, pkt_id);
	switch (type)
	{
	case FIRMWARE_SOFTWARE_TYPE::BOOT_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_BOOT_VERSION);
		break;
	case FIRMWARE_SOFTWARE_TYPE::MAIN_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_MAIN_VERSION);
		break;
	case FIRMWARE_SOFTWARE_TYPE::USER_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_USER_VERSION);
		break;
	case FIRMWARE_SOFTWARE_TYPE::CAMERA_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_GET_CAMERA_VERSION);
		break;
	default:
		break;
	}
	//由于发送的data部分前8个字节固定为offset和length，固此处需要添加改8个字节作为总长度
	c_data.data_len_ = file_block_data.size() + FIRMWARE_RW_FILE_BLOCK_LENGTH;
	//添加offset数据，length数据
	char off_len[8];
	uint32_t cb = file_block_data.size();
	memcpy(off_len,  &block_offset, sizeof(block_offset));
	memcpy(&off_len[sizeof(block_offset)], &cb, sizeof(cb));
	c_data.data_.assign(off_len, sizeof(off_len));
	c_data.data_.append(file_block_data.substr(0, file_block_data.size()));
	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_vcu_read_bin_file(const FIRMWARE_SOFTWARE_TYPE type, const nsp::tcpip::endpoint& ep, const int block_offset,
	const int buffer_length, const std::shared_ptr<motion::asio_block>& asio_ack){
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_RW_FIRMWARE, pkt_id);
	switch (type)
	{
	case FIRMWARE_SOFTWARE_TYPE::BOOT_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_READ_BOOT_FILE);
		break;
	case FIRMWARE_SOFTWARE_TYPE::MAIN_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_READ_MAIN_FILE);
		break;
	case FIRMWARE_SOFTWARE_TYPE::USER_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_READ_USER_FILE);
		break;
	case FIRMWARE_SOFTWARE_TYPE::CAMERA_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_READ_CAMERA_FILE);
		break;
	case FIRMWARE_SOFTWARE_TYPE::COMPARE_SOFTWARE:
		c_data.set_sub_operate(FIRMWARE_SUB_OPERATE_CODE_READ_NORAML_FILE);
		break;
	default:
		break;
	}
	//此处长度固定为8个字节
	c_data.data_len_ = FIRMWARE_RW_FILE_BLOCK_LENGTH;
	//添加offset数据，length数据
	char data_buffer[8];
	memcpy(data_buffer, &block_offset, sizeof(block_offset));
	memcpy(&data_buffer[sizeof(block_offset)], &buffer_length, sizeof(buffer_length));
	c_data.data_.assign(data_buffer, sizeof(data_buffer));
	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_vcu_keep_alive_request(const nsp::tcpip::endpoint& ep)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_KEEPALIVE, FIRMWARE_SUB_OPERATE_CODE_GET_VCU_TYPE, pkt_id);
	c_data.data_len_ = 0;
	return psend(c_data,ep);
}

int udp_session::post_query_vcu_keep_alive_request(const nsp::tcpip::endpoint& ep, const std::shared_ptr<motion::asio_block> &asio_ack)
{
	loinfo("agv_shell") << "udp:send query vcu keep alive request,the target endpoint is " << ep.to_string();
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_KEEPALIVE, FIRMWARE_SUB_OPERATE_CODE_GET_KEEPALIVE_STATUS, pkt_id);
	c_data.data_len_ = 0;
	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]()->int{
		return psend(c_data,ep);
	}, asio_ack);
}

int udp_session::post_set_vcu_keep_alive_request(const nsp::tcpip::endpoint& ep, int status, const std::shared_ptr<motion::asio_block> &asio_ack)
{
	loinfo("agv_shell") << "udp:send set vcu keep alive request,the target endpoint is " << ep.to_string();
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	if (0 == status) {
		nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_KEEPALIVE, FIRMWARE_SUB_OPERATE_CODE_SET_KEEPALIVE_START, pkt_id);
		c_data.data_len_ = 0;
		return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]()->int{
			return psend(c_data,ep);
		}, asio_ack);
	} else {
		nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_KEEPALIVE, FIRMWARE_SUB_OPERATE_CODE_SET_KEEPALIVE_SUSPEND, pkt_id);
		c_data.data_len_ = 0;
		return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]()->int{
			return psend(c_data,ep);
		}, asio_ack);
	}
}

int udp_session::psend(const nsp::proto::proto_interface& package, const nsp::tcpip::endpoint& ep){
	return obudp::sendto(package.length(), [&](void*buffer, int cb)->int{
		return (package.serialize((unsigned char*)buffer) < 0) ? -1 : 0;
	}, ep);
}


/*****************************************************************************************************************************************/

//以下为can自主固件升级协议
int udp_session::post_start_data_foward(const nsp::tcpip::endpoint& ep, uint8_t code_serial, const std::shared_ptr<motion::asio_block> &asio_ack)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_CAN_COMMOND_START_DATA_FORWARD, code_serial, pkt_id);
	c_data.data_len_ = 0;

	return nsp::toolkit::singleton<net_manager>::instance()->write(pkt_id, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_type_request(const nsp::tcpip::endpoint& ep, const uint8_t nodeid, 
	const uint8_t code_serial,const std::shared_ptr<motion::asio_block> &asio_ack)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_GET_INFO, FIRMWARE_SUB_OPERATE_CODE_GET_VCU_TYPE, pkt_id);

	c_data.head_.operate_ = FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE;
	c_data.head_.sub_operate_ = code_serial;
	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_GET_HARDWARE_TYPE, 0, nodeid);
	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	pkt_id = FIRMWARE_CAN_COMMOND_GET_HARDWARE_TYPE_ACK;

	delete[] pbuffer;
	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_GET_HARDWARE_TYPE_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_version_request(const nsp::tcpip::endpoint& ep, const uint8_t nodeid,
	const uint8_t code_serial, const std::shared_ptr<motion::asio_block> &asio_ack)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_GET_INFO, pkt_id);

	c_data.head_.operate_ = FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE;
	c_data.head_.sub_operate_ = code_serial;
	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_GET_SOFTWARE_VERSION, 0, nodeid);
	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;
	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_GET_SOFTWARE_VERSION_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_reset_request(const nsp::tcpip::endpoint& ep, const uint8_t node_id,
	const uint8_t code_serial, const std::shared_ptr<motion::asio_block> &asio_ack)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE, code_serial, pkt_id);
	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_RESET, 0, node_id);

	can_data.data_length = 0;

	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;

	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_RESET_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_abort_update_request(const nsp::tcpip::endpoint& ep, uint8_t nodeid, uint8_t code_serial,
	const std::shared_ptr<motion::asio_block> &asio_ack)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE, code_serial, pkt_id);

	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_ABORT_UPDATE, 0, nodeid);

	can_data.data_length = 0;

	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;

	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_ABORT_UPDATE_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_write_bin_file(const nsp::tcpip::endpoint& ep, const int block_offset, const std::string&file_block_data,
	const std::shared_ptr<motion::asio_block>& asio_ack, uint8_t code_serial, uint8_t nodeid)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE, code_serial, pkt_id);

	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_DOWNLOAD_PACKET, block_offset, nodeid);
	can_data.data_.assign(file_block_data.c_str(), file_block_data.size());
	can_data.data_length = file_block_data.size();

	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;

	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_DOWNLOAD_PACKET_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_read_bin_file(const nsp::tcpip::endpoint& ep, const int block_offset,
	const std::shared_ptr<motion::asio_block>& asio_ack, uint8_t code_serial, uint8_t nodeid)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE, code_serial, pkt_id);

	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_UPLOAD_PACKET, block_offset, nodeid);
	can_data.data_length = 0;
	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;

	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_UPLOAD_PACKET_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_read_register_request(const nsp::tcpip::endpoint& ep, uint8_t code_serial,
	const std::shared_ptr<motion::asio_block> &asio_ack, uint16_t register_index, uint8_t nodeid)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE, code_serial, pkt_id);

	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_READ_REGISTER, register_index, nodeid);
	can_data.data_length = 0;
	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;

	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_READ_REGISTER_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_write_register_request(const nsp::tcpip::endpoint& ep, uint8_t code_serial,
	const std::shared_ptr<motion::asio_block> &asio_ack, uint16_t register_index, uint8_t nodeid, std::string strdata)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE, code_serial, pkt_id);

	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_WRITE_REGISTER, register_index, nodeid);
	can_data.data_length = 0;
	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;

	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_WRITE_REGISTER_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}

int udp_session::post_can_init_rom_request(const nsp::tcpip::endpoint& ep, const uint16_t block_number, uint8_t nodeid, uint8_t code_serial,
	const std::shared_ptr<motion::asio_block> &asio_ack)
{
	uint16_t pkt_id = nsp::toolkit::singleton<net_manager>::instance()->pkt_id();
	nsp::proto::udp::common_data c_data(FIRMWARE_MAIN_OPERATE_CODE_CAN_DATA_CHANGE, code_serial, pkt_id);
	c_data.data_len_ = 0;

	nsp::proto::udp::can_data_t can_data(FIRMWARE_CAN_COMMOND_INITIAL_ROM_UPDATE, 0, nodeid);
	uint16_t block_offset = 1;
	can_data.data_.append((char *)&block_offset, sizeof(block_offset));
	can_data.data_.append((char *)&block_number, sizeof(block_number));
	can_data.data_length = 4;

	c_data.data_len_ = can_data.length();
	unsigned char *pbuffer = nullptr;
	try
	{
		pbuffer = new unsigned char[c_data.data_len_];
	}
	catch (...)
	{
		return -1;
	}

	can_data.serialize(pbuffer);
	c_data.data_.assign((char *)pbuffer, c_data.data_len_);
	delete[] pbuffer;

	return nsp::toolkit::singleton<net_manager>::instance()->write_data_forward(FIRMWARE_CAN_COMMOND_INITIAL_ROM_UPDATE_ACK, [&]() ->int {
		return psend(c_data, ep);
	}, asio_ack);
}