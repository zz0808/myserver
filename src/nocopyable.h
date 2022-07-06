#ifndef NOCOPYABLE_H_
#define NOCOPYABLE_H_

namespace server {

class Nocopyable {
public:
    Nocopyable() = default;
    ~Nocopyable() = default;
    Nocopyable(const Nocopyable&) = delete;
    Nocopyable& operator=(const Nocopyable&) = delete;
};

} // namespace server
#endif