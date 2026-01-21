#ifndef TEST_HPP
#define TEST_HPP

namespace MesaOS::Apps {

class Test {
public:
    static void run(const char* arg = nullptr);
    static bool is_running();

private:
    static bool running;
};

} // namespace MesaOS::Apps

#endif
