FROM gcc:14@sha256:a257536c9b7a0ee193c1880f2d691358d2c9760ef0ede3ee741aa243789743c7
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq cmake && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
CMD ["bash", "-lc", "set -e; cmake -S tests/host -B build/host; cmake --build build/host; ctest --test-dir build/host --output-on-failure"]