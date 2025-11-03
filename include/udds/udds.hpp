/* source code: <https://github.com/shynur/udds> */
#pragma once
#include <bits/stdc++.h>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
using namespace std::literals;

namespace shynur::udds {
    /**
     * @tparam proto_t 由 IDL 文件所定义的消息类型 转换为 C++ class 后 的 class 类型.
     * @tparam proto_pub_sub_t 给 proto_t 加上 'PubSubType' 的后缀而已.
     * @tparam proto_name_cstr 返回 proto_t 的类型名, 以字符串的形式.
     */
    template <
        #ifdef __cpp_lib_concepts
            std::regular
        #else
            typename
        #endif
                     proto_t,
        #ifdef __cpp_lib_concepts
            std::derived_from<::eprosima::fastdds::dds::TopicDataType>
        #else
            typename
        #endif
                     proto_pub_sub_t
        #ifndef SEER_ROBOTICS_UDDS
            , std::regular_invocable<> auto proto_name_cstr
        #endif
    >
    #ifndef SEER_ROBOTICS_UDDS
        requires requires {
            { proto_name_cstr() } -> std::same_as<const char *>;
        }
    #endif
    class Publisher {
        ::eprosima::fastdds::dds::DomainParticipant *participant = nullptr;
        ::eprosima::fastdds::dds::Publisher *publisher = nullptr;
        ::eprosima::fastdds::dds::Topic *topic = nullptr;
        ::eprosima::fastdds::dds::DataWriter *writer = nullptr;
        ::eprosima::fastdds::dds::TypeSupport type;
        struct: ::eprosima::fastdds::dds::DataWriterListener {
            std::atomic_int matched{0};  // TODO: 可以改成 uint 吗? 进一步地, uchar 应该绰绰有余了.

            void on_publication_matched(
                ::eprosima::fastdds::dds::DataWriter *,
                const ::eprosima::fastdds::dds::PublicationMatchedStatus& info
            ) override {
                switch (info.current_count_change) {
                    case 1:
                        // Publisher matched.
                        this->matched = info.total_count;
                        break;
                    case -1:
                        // Publisher unmatched.
                        this->matched = info.total_count;
                        break;
                    default:
                        std::cerr <<
                            "info.current_count_change=" + std::to_string(info.current_count_change) + "is not a valid value for "
                            "PublicationMatchedStatus current count change.\n";
                }
            }
        } writer_listener;

      public:
        /**
         * @param domain_id 发布订阅的 domain, 同一个 domain 之间的 topic 是可见的.
         * @param participant_name publisher 的 name.  (看日志的时候有用.)
         * @param topic_name topic 的 name.  不需要和 proto 定义时的类型名字相同, 随便写一个就行.  (看日志的时候有用.)
         */
        Publisher(
            const std::uint8_t domain_id,
            const std::string participant_name,
            const std::string topic_name
            #ifdef SEER_ROBOTICS_UDDS
            , const char *(* const proto_name_cstr)()
            #endif
        ): type{new proto_pub_sub_t} {
            this->participant
                = ::eprosima::fastdds::dds::DomainParticipantFactory::get_instance()
                    ->create_participant(
                        domain_id,
                        [&] {
                            auto qos = ::eprosima::fastdds::dds::DomainParticipantQos{};
                            qos.name(participant_name.c_str());
                            return qos;
                        }()
                    );
            if (!this->participant)
                goto failed_init;

            this->type.register_type(this->participant);

            this->topic
                = this->participant->create_topic(
                    topic_name.c_str(),
                    proto_name_cstr(),
                    [] {
                        auto qos = ::eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;
                        qos.reliability().max_blocking_time = 1.0;  // 可靠传输需要允许阻塞比较长的时间.
                        return qos;
                    }()
                );
            if (!this->topic)
                goto failed_init;

            this->publisher
                = this->participant->create_publisher(
                    ::eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT
                );
            if (!this->publisher)
                goto failed_init;

            this->writer
                = this->publisher->create_datawriter(
                    topic,
                    ::eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT,
                    &this->writer_listener
                );
            if (!this->writer)
                goto failed_init;

            if (false) {
                failed_init:
                    [[unlikely]];
                    throw std::runtime_error{  // TODO: 更合适的错误类型
                        "Failed to initialize Publisher"  // TODO: 更详细的错误信息
                    };
            }
        }
        ~Publisher() {
            if (this->writer)
                this->publisher->delete_datawriter(this->writer);

            if (this->publisher)
                this->participant->delete_publisher(this->publisher);

            if (this->topic)
                this->participant->delete_topic(this->topic);

            ::eprosima::fastdds::dds::DomainParticipantFactory::get_instance()
            ->delete_participant(this->participant);
        }

        /**
         * @brief 如有必要, 则发布消息.
         *        如果未发现相应的 reader, 则不发布 (因为没有人会接收).
         * @return 如果真的发布了消息, 则返回 true; 否则返回 false.
         */
        #ifdef SEER_ROBOTICS_UDDS
        template <typename Thunk>
        #endif
        auto publish(
       	    #ifndef SEER_ROBOTICS_UDDS
	    const proto_t&
	    #else
	    Thunk&&
	    #endif
                    message
	) {
            if (this->writer_listener.matched >= 1) [[likely]] {
                #ifdef SEER_ROBOTICS_UDDS
                    const auto&& msg = message();
                #endif
                this->writer->write(
                    &
                    #ifndef SEER_ROBOTICS_UDDS
                     message
                    #else
                     msg
                    #endif
                );
                return true;
            } else
                return false;
        }
    };

    /**
     * @tparam proto_t 由 IDL 文件所定义的消息类型 转换为 C++ class 后 的 class 类型.
     * @tparam proto_pub_sub_t 给 proto_t 加上 'PubSubType' 的后缀而已.
     * @tparam proto_name_cstr proto_t 的类型名, 以字符串的形式.
     */
    template <
        #ifdef __cpp_lib_concepts
            std::regular
        #else
            typename
        #endif
                     proto_t,
        #ifdef __cpp_lib_concepts
            std::derived_from<::eprosima::fastdds::dds::TopicDataType>
        #else
            typename
        #endif
                     proto_pub_sub_t
        #ifndef SEER_ROBOTICS_UDDS
            , std::regular_invocable<> auto proto_name_cstr
        #endif
    >
    #ifndef SEER_ROBOTICS_UDDS
        requires requires {
            { proto_name_cstr() } -> std::same_as<const char *>;
        }
    #endif
    class Subscriber {
        ::eprosima::fastdds::dds::DomainParticipant *participant = nullptr;
        ::eprosima::fastdds::dds::Subscriber *subscriber = nullptr;
        ::eprosima::fastdds::dds::DataReader *reader = nullptr;
        ::eprosima::fastdds::dds::Topic *topic = nullptr;
        ::eprosima::fastdds::dds::TypeSupport type;
        struct ReaderListener: ::eprosima::fastdds::dds::DataReaderListener {
	    #ifdef __cpp_lib_move_only_function
                std::move_only_function
	    #else
		std::function
	    #endif
		             <
                std::unique_ptr<proto_t, std::function<void(proto_t *)>>()
            > message_locator;
            #ifdef __cpp_lib_move_only_function
                std::move_only_function
	    #else
		std::function
	    #endif
		             <void(proto_t&)> message_processor;

            ReaderListener(
                decltype(ReaderListener::message_locator) message_locator,
                decltype(ReaderListener::message_processor) message_processor
            ): message_locator{std::move(message_locator)},
               message_processor{std::move(message_processor)} {}

            void on_subscription_matched(
                ::eprosima::fastdds::dds::DataReader *,
                const ::eprosima::fastdds::dds::SubscriptionMatchedStatus& info
            ) override {
                switch (info.current_count_change) {
                    case 1:
                        // Subscriber matched.
                        break;
                    case -1:
                        // Subscriber unmatched.
                        break;
                    default:
                        std::cerr <<
                            "info.current_count_change={" + std::to_string(info.current_count_change) + "} is not a valid value "
                            "for SubscriptionMatchedStatus current count change.\n";
                }
            }
            void on_data_available(::eprosima::fastdds::dds::DataReader *const reader) override {
                auto info = ::eprosima::fastdds::dds::SampleInfo{};
                auto message = this->message_locator();

                if (
                    reader->take_next_sample(&*message, &info)
                    == ::eprosima::fastdds::dds::RETCODE_OK
                ) [[likely]]
                    if (info.valid_data) [[likely]] {
                        this->message_processor(*message);
                        message.release();
                        return;
                    }
                std::cerr << "监听到有数据到来, 但未能成功读取.\n";
            }
        } reader_listener;

      public:
        /**
         * @param domain_id 发布订阅的 domain, 同一个 domain 之间的 topic 是可见的.
         * @param participant_name subscriber 的 name.  (看日志的时候有用.)
         * @param topic_name topic 的 name.  不需要和 proto 定义时的类型名字相同, 随便写一个就行.  (看日志的时候有用.)
         * @param message_locator 一个 callback, 返回 `std::unique_ptr<proto_t, deleter_type>`.
         *                        Reader 会把接收到的消息填充到 `*unique_ptr`.  Reader 每次发现
         *                        有新消息到来, 都会同步调用它, 因此需要保证该 callback 的调用是
         *                        足够迅速的.
         *                        如果成功向 `*unique_ptr` 填充了消息, 则 unique_ptr 释放所有权,
         *                        使得消息保留在内存中; 否则, 调用 `deleter_type`.
         * @param message_processor 一个 callback, 接收一个 proto_t 的引用.  每次 reader 接收到消息后
         *                          都会同步调用它.  因此需要保证该 callback 的调用是足够迅速的.
         */
        #ifndef __cpp_lib_concepts
            template <typename F>
        #endif
        Subscriber(
            const std::uint8_t domain_id,
            const std::string participant_name,
            const std::string topic_name,
            #ifndef SEER_ROBOTICS_UDDS
                std::invocable<> auto&& message_locator,
	    #endif
            #ifdef __cpp_lib_concepts
                std::invocable<
		    #ifndef SEER_ROBOTICS_UDDS
			proto_t&
		    #else
	                std::shared_ptr<proto_t>
                    #endif
	        > auto
            #else
                F
	    #endif
		 && message_processor
            #ifdef SEER_ROBOTICS_UDDS
            , const char *(* const proto_name_cstr)()
            #endif
        )
	#ifndef SEER_ROBOTICS_UDDS
            requires requires {
                std::unique_ptr{message_locator()};
                requires std::is_same_v<proto_t, typename decltype(message_locator())::element_type>;
            }
        #endif
        : type{new proto_pub_sub_t},
           reader_listener{
	    #ifndef SEER_ROBOTICS_UDDS
                std::forward<decltype(message_locator)>(message_locator)
            #else
                std::make_unique<proto_t>
            #endif
                ,
	    [msg_processor=std::forward<decltype(message_processor)>(message_processor)](proto_t& msg) mutable {
                msg_processor(
	            #ifndef SEER_ROBOTICS_UDDS
                        msg
                    #else
		        std::shared_ptr<std::decay_t<decltype(msg)>>{&msg}
                    #endif
		);
	    }
        } {
            this->participant
                = ::eprosima::fastdds::dds::DomainParticipantFactory::get_instance()
                    ->create_participant(
                        domain_id,
                        [&] {
                            auto participant_qos = ::eprosima::fastdds::dds::DomainParticipantQos{};
                            participant_qos.name(participant_name.c_str());
                            return participant_qos;
                        }()
                    );
            if (!this->participant)
                goto failed_init;

            this->type.register_type(this->participant);

            this->topic
                = this->participant->create_topic(
                    topic_name.c_str(),
		    proto_name_cstr(),
                    [] {
                        auto qos = ::eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;
                        qos.durability().kind  // 订阅该主题后自动获取历史消息.
                            = ::eprosima::fastdds::dds::DurabilityQosPolicyKind::TRANSIENT_LOCAL_DURABILITY_QOS;
                        qos.reliability().kind  // 丢失的消息会被重新传输过来.
                            = ::eprosima::fastdds::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS;
                        qos.reliability().max_blocking_time = 1.0;  // 可靠传输需要允许阻塞比较长的时间.
                        return qos;
                    }()
                );
            if (!this->topic)
                goto failed_init;

            this->subscriber
                = this->participant->create_subscriber(
                    ::eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT
                );
            if (!this->subscriber)
                goto failed_init;

            this->reader
                = this->subscriber->create_datareader(
                    this->topic,
                    ::eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT,
                    &this->reader_listener
                );
            if (!this->reader)
                goto failed_init;

            if (false) {
                failed_init:
                    [[unlikely]];
                    throw std::runtime_error{  // TODO: 更合适的错误类型
                        "Failed to initialize Publisher"  // TODO: 更详细的错误信息
                    };
            }
        }
        ~Subscriber() {
            if (this->reader)
                this->subscriber->delete_datareader(this->reader);

            if (this->topic)
                this->participant->delete_topic(this->topic);

            if (this->subscriber)
                this->participant->delete_subscriber(this->subscriber);

            ::eprosima::fastdds::dds::DomainParticipantFactory::get_instance()
            ->delete_participant(this->participant);
        }
    };
}

#ifdef SEER_ROBOTICS_UDDS
    #define RBK_UDDS_PUBLISHER(channel, topic_name, proto_typename)                     \
                ::shynur::udds::Publisher<proto_typename, proto_typename##PubSubType>{  \
                    channel, std::to_string(std::rand()), topic_name,                   \
                    +[] {return #proto_typename;}                                       \
                }
    #define RBK_UDDS_SUBSCRIBER(channel, topic_name, proto_typename, callback)           \
                ::shynur::udds::Subscriber<proto_typename, proto_typename##PubSubType>{  \
                    channel, std::to_string(std::rand()), topic_name, callback,          \
                    +[] {return #proto_typename;}                                        \
                }
#endif

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// tab-width: 8
// End:
