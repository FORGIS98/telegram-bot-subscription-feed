cmake_minimum_required(VERSION 3.10)
project(TelegramYouTubeBot)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

include_directories(
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    /usr/local/include  # para tgbot headers
)

link_directories(/usr/local/lib)  # para tgbot lib

add_executable(TelegramYouTubeBot src/main.cpp)

target_link_libraries(TelegramYouTubeBot
    /usr/local/lib/libTgBot.a
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    Threads::Threads
)
