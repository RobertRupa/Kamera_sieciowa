#include "camera_hub/ProfilePolicy.hpp"

#include <cstdio>
#include <cstdlib>

using namespace camera_hub;

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}

int main() {
    const VideoProfile supported[]{{640, 480, 15, 80}, {1280, 720, 15, 80}};
    const auto current = supported[0];
    const ProfilePolicy policy(supported, 2);
    require(policy.check(current, current, false) == ProfileResult::Accepted, "supported profile");
    require(policy.check(current, current, true) == ProfileResult::Accepted, "idempotent update");
    require(policy.check(supported[1], current, true) == ProfileResult::Busy, "USB profile conflict");
    require(policy.check(supported[1], current, false) == ProfileResult::Accepted, "idle reconfigure");
    require(policy.check({640, 480, 15, 60}, current, true) == ProfileResult::Busy, "quality frozen on USB");
    require(policy.check({640, 480, 15, 60}, current, false) == ProfileResult::Accepted, "valid quality");
    require(policy.check({1920, 1080, 30, 80}, current, false) == ProfileResult::Unsupported,
            "unlisted format rejected");
    require(policy.check({0, 480, 15, 80}, current, false) == ProfileResult::InvalidArgument, "zero width");
    require(policy.check({640, 480, 0, 80}, current, false) == ProfileResult::InvalidArgument, "zero FPS");
    require(policy.check({640, 480, 15, 101}, current, false) == ProfileResult::InvalidArgument, "quality bounds");
    require(ProfilePolicy(nullptr, 2).check(current, current, false) == ProfileResult::Unverified,
            "unknown sensor has no usable profiles");
    require(policy.negotiateUsb(1280, 720, 15, current) == ProfileResult::Busy,
            "USB cannot choose separate profile");
    require(policy.negotiateUsb(640, 480, 15, current) == ProfileResult::Accepted, "matching USB request");
    std::puts("PASS: shared profile validation and USB negotiation conflicts");
}