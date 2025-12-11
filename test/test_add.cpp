#include <cassert>
#include "lib.h"

int main() {
    assert(kv::add(2, 3) == 5);
    assert(kv::add(-1, 1) == 0);
    return 0;
}
