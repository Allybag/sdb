#include <sys/signal.h>

#include <cstdio>
#include <cstdint>

int main()
{
    std::uint64_t data = 0xcafecafe;
    auto data_address = &data;

    write(STDOUT_FILENO, &data_address, sizeof(void*));
    fflush(stdout);

    raise(SIGTRAP);

    char str[12] = {0};
    auto str_address = &str;

    write(STDOUT_FILENO, &str_address, sizeof(void*));
    fflush(stdout);

    raise(SIGTRAP);

    printf("%s", str);
}
