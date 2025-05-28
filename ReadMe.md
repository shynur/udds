## 前置设置

广播系统 在 局域网 下创建.
任何初始化了 broadcast 模块的进程之间都可以相互发现.

如何在一个进程内初始化 broadcast 模块?  执行:

```C++
rbk::udds::broadcast::init(
    /* robot_id: */ "你作为广播系统的参与者需要给自己取一个名字"
);
```

(只要 **进程** 之间的 `robot_id` 不同, 这些进程就都可以加入到广播系统中, 所以一台车可以加好几个进程进去.)

广播系统的参与者数量没有上限.

## 消息格式

```C++
struct UddsJsonProto {
    auto timestamp() -> u64&;  // 发布时的时间/纳秒
    auto delay() -> u64&;      // 网络延迟/纳秒

    auto robot_id() -> string&;
    auto x() -> double&;
    auto y() -> double&;
    auto theta() -> double&;

    auto json() -> string&;
};
```

## 发布

向局域网中目前已经被发现的订阅者广播消息:

```C++
auto message = UddsJsonProto{};

// 设置消息内容.  例如:
message.json() = "[1,2,3]";

send(message);
```

`message` 的 `timestamp` / `delay` / `robot_id` 字段会被自动设置.

## 订阅

Broadcast 模块把收到的最新的消息存储在 `rbk::udds::broadcast::received_from` 中.
它只保留每个发布者最新的消息.

### 统计消息数量

```C++
std::size(rbk::udds::broadcast::received_from)
```

### 判断是否有来自指定小车的消息

```C++
bool received_from_car_ROBOID =
    rbk::udds::broadcast::received_from.contains("ROBOID");
```

### 获取来自指定小车的消息

```C++
try {
    auto msg_by_ROBOID = rbk::udds::broadcast::received_from["ROBOID"];
} catch (const std::out_of_range&) {}
```

or

```C++
assert(rbk::udds::broadcast::received_from.contains("ROBOID"));
auto msg_by_ROBOID = rbk::udds::broadcast::received_from["ROBOID"];
```

**必须通过单独的变量 (以上是 `msg_by_ROBOID`) 访问消息**, 这是并发安全的.

#### 读取消息

```C++
std::cout << msg_by_ROBOID->timestamp() << '\n'
          << msg_by_ROBOID->json() << '\n' // ... ...
```

## 列出发布过消息的小车的名单

```C++
auto robots = rbk::udds::broadcast::received_from.keys();
for (auto robot_id : robots)
    std::cout << *robot_id << '\n';
```

## loop

```C++
for (auto [robot_id, msg] : rbk::udds::broadcast::received_from) {
    std::cout << *robot_id << ": \n"
              << msg->timestamp() << '\n'
              << msg->json() << '\n';
    // ... 处理消息 ...
}
```
