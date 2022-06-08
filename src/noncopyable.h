#ifndef NONCOPYABLE_H_
#define NONCOPYABLE_H_

namespace dserver {

class Noncopyable {
public:
    Noncopyable() = default;
    ~Noncopyable() = default;
    Noncopyable& operator=(const Noncopyable&) = delete;
    Noncopyable(const Noncopyable&) = delete;
};

} // namespace dserver

#endif