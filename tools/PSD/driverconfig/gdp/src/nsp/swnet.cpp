﻿#include <exception>

#include "swnet.h"
#include "os_util.hpp"

namespace nsp {
    namespace tcpip {

        void STD_CALL swnet::tcp_io(const nis_event_t *tcp_evt, const void *data) {
            if (!tcp_evt || !data) return;

            tcp_data_t *tcp_dat = (tcp_data_t *) data;

            switch (tcp_evt->Event) {
                case EVT_RECEIVEDATA:{
                    toolkit::singleton<swnet>::instance()->tcp_refobj(tcp_evt->Ln.Tcp.Link, [&] (const std::shared_ptr<obtcp> &object) {
                        object->on_recvdata(tcp_dat->e.Packet.Data, tcp_dat->e.Packet.Size);
                    });
                }
                    break;
                case EVT_TCP_ACCEPTED:
                    toolkit::singleton<swnet>::instance()->tcp_refobj(tcp_evt->Ln.Tcp.Link, [&] (const std::shared_ptr<obtcp> &object) {
                        object->on_accepted(tcp_evt->Ln.Tcp.Link, tcp_dat->e.Accept.AcceptLink);
                    });
                    break;
				case EVT_TCP_CONNECTED:
					toolkit::singleton<swnet>::instance()->tcp_refobj(tcp_evt->Ln.Tcp.Link, [&](const std::shared_ptr<obtcp> &object) {
						object->on_connected();
					});
					break;
                case EVT_CLOSED:
                    toolkit::singleton<swnet>::instance()->tcp_refobj(tcp_evt->Ln.Tcp.Link, [&] (const std::shared_ptr<obtcp> &object) {
                        object->on_closed();
                    });
                    break;
                case EVT_DEBUG_LOG:
                     toolkit::singleton<swnet>::instance()->tcp_refobj(tcp_evt->Ln.Tcp.Link, [&] (const std::shared_ptr<obtcp> &object) {
                        object->on_debug_output(tcp_dat->e.DebugLog.logstr);
                    });
                    break;
                default:
                    break;
            }
        }

        void STD_CALL swnet::udp_io(const nis_event_t *udp_evt, const void *data) {
            if (!udp_evt || !data) return;

            udp_data_t *udp_dat = (udp_data_t *) data;

            switch (udp_evt->Event) {
                case EVT_RECEIVEDATA:
                    toolkit::singleton<swnet>::instance()->udp_refobj(udp_evt->Ln.Udp.Link, [&] (const std::shared_ptr<obudp> &object) {
                        object->on_recvdata(udp_dat->e.Packet.Data, udp_dat->e.Packet.Size,
                                udp_dat->e.Packet.RemoteAddress,
                                udp_dat->e.Packet.RemotePort);
                    });
                    break;
                case EVT_CLOSED:
                    toolkit::singleton<swnet>::instance()->udp_refobj(udp_evt->Ln.Udp.Link, [&] (const std::shared_ptr<obudp> &object) {
                        object->on_closed();
                    });
                    break;
                case EVT_DEBUG_LOG:
                     toolkit::singleton<swnet>::instance()->udp_refobj(udp_evt->Ln.Udp.Link, [&] (const std::shared_ptr<obudp> &object) {
                        object->on_debug_output(udp_dat->e.DebugLog.logstr);
                    });
                    break;
                default:
                    break;
            }
        }

        swnet::swnet() {
#if _WIN32
            shared_library_ = os::dlopen("nshost.dll");
#else
            shared_library_ = os::dlopen("nshost.so");
            if (!shared_library_) {
                printf(dlerror());
                throw std::exception();
            }
#endif  
        }

        swnet::~swnet() {
            if (shared_library_) {
                os::dlclose(shared_library_);
            }
        }

        int swnet::tcp_create(std::shared_ptr<obtcp> object, const char *ipstr, const port_t port) {
            auto lnk = tcp_create(&swnet::tcp_io, ipstr, port);
            if (INVALID_HTCPLINK == lnk) {
                return -1;
            }

            object->setlnk(lnk);

            std::lock_guard < decltype(this->lock_tcp_redirection_) > guard(lock_tcp_redirection_);
            auto iter = tcp_object_.find(lnk);
            if (tcp_object_.end() == iter) {
                tcp_object_[lnk] = object;
                return 0;
            }
            return -1;
        }

        int swnet::tcp_attach(HTCPLINK lnk, const std::shared_ptr<obtcp> &object) {
            std::lock_guard < decltype(lock_tcp_redirection_) > guard(lock_tcp_redirection_);
            auto iter = tcp_object_.find(lnk);
            if (tcp_object_.end() == iter) {
                tcp_object_[lnk] = object;
                return 0;
            }
            return -1;
        }

        void swnet::tcp_detach(HTCPLINK lnk) {
            std::lock_guard < decltype(lock_tcp_redirection_) > guard(lock_tcp_redirection_);
            auto iter = tcp_object_.find(lnk);
            if (tcp_object_.end() != iter) {
                tcp_object_.erase(iter);
            }
        }

        int swnet::tcp_search(const HTCPLINK lnk, std::shared_ptr<obtcp> &object) const {
            std::lock_guard < decltype(lock_tcp_redirection_) > guard(lock_tcp_redirection_);
            auto iter = tcp_object_.find(lnk);
            if (tcp_object_.end() != iter) {
                object = iter->second;
                return 0;
            }
            return -1;
        }

        void swnet::tcp_refobj(const HTCPLINK lnk, const std::function<void( const std::shared_ptr<obtcp>)> &todo) {
            if (INVALID_HTCPLINK != lnk && todo) {
                std::shared_ptr<obtcp> object;
                if (tcp_search(lnk, object) >= 0) {
                    todo(object);
                }
            }
        }
        ///////////////////////////////////////////////////////////   UDP /////////////////////////////////////////////////////////////

        int swnet::udp_create(std::shared_ptr<obudp> object, const char* ipstr, const port_t port, int flag) {
            auto lnk = udp_create(&swnet::udp_io, ipstr, port, flag);
            if (INVALID_HUDPLINK == lnk) {
                return -1;
            }

            object->setlnk(lnk);
            std::lock_guard < decltype(this->lock_udp_redirection_) > guard(lock_udp_redirection_);
            auto iter = udp_object_.find(lnk);
            if (udp_object_.end() == iter) {
                udp_object_[lnk] = object;
                return 0;
            }
            return -1;
        }

        void swnet::udp_detach(HUDPLINK lnk) {
            std::lock_guard < decltype(lock_udp_redirection_) > guard(lock_udp_redirection_);
            auto iter = udp_object_.find(lnk);
            if (udp_object_.end() != iter) {
                udp_object_.erase(iter);
            }
        }

        int swnet::udp_search(const HUDPLINK lnk, std::shared_ptr<obudp> &object) const {
            std::lock_guard < decltype(lock_udp_redirection_) > guard(lock_udp_redirection_);
            auto iter = udp_object_.find(lnk);
            if (udp_object_.end() != iter) {
                object = iter->second;
                return 0;
            }
            return -1;
        }

        void swnet::udp_refobj(const HUDPLINK lnk, const std::function<void( const std::shared_ptr<obudp>)> &todo) {
            if (INVALID_HUDPLINK != lnk && todo) {
                std::shared_ptr<obudp> object;
                if (udp_search(lnk, object) >= 0) {
                    todo(object);
                }
            }
        }

    }
}