# 消息格式

```C++
struct UddsJsonProto {
    /*** getters & setters ***/
    auto timestamp() -> double&;       // 消息发布时间
    auto host_id() -> std::string&;    // 标识小车
    auto seq_num() -> std::uint32_t&;  // 消息序列号 (随机): 区分同一小车的不同消息
    auto json() -> std::string&;
};
```

# 广播系统

广播域的参与者需要名字, 通过定义

```C++
auto rbk::udds::broadcast::get_participant_name() -> std::string;
```

以允许运行时指定名字.
