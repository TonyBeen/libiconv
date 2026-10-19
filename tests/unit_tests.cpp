#include <catch2/catch_test_macros.hpp>
#include <iconv.h>
#include <localcharset.h>
#include <string>
#include <vector>
#include <cerrno>
#include <cstring>

static bool convert_string(const std::string& tocode, const std::string& fromcode,
                           const std::string& input, std::string& output) {
    iconv_t cd = iconv_open(tocode.c_str(), fromcode.c_str());
    if (cd == (iconv_t)(-1)) {
        return false;
    }

    size_t inbytesleft = input.size();
    char* inbuf = const_cast<char*>(input.data());

    std::vector<char> outvec(input.size() * 4 + 64, '\0');
    size_t outbytesleft = outvec.size() - 1;
    char* outbuf = outvec.data();

    size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    iconv_close(cd);

    if (res == (size_t)(-1)) {
        return false;
    }

    output.assign(outvec.data(), outbuf - outvec.data());
    return true;
}

TEST_CASE("iconv_open and iconv_close basic lifecycle", "[lifecycle]") {
    SECTION("Valid encodings") {
        iconv_t cd = iconv_open("UTF-8", "ASCII");
        REQUIRE(cd != (iconv_t)(-1));
        int res = iconv_close(cd);
        REQUIRE(res == 0);
    }

    SECTION("Invalid encoding returns error and sets errno to EINVAL") {
        errno = 0;
        iconv_t cd = iconv_open("INVALID_NON_EXISTENT_ENCODING", "UTF-8");
        REQUIRE(cd == (iconv_t)(-1));
        REQUIRE(errno == EINVAL);
    }
}

TEST_CASE("ASCII and UTF-8 self conversion", "[basic]") {
    std::string in = "Hello, libiconv world! 1234567890";
    std::string out;
    REQUIRE(convert_string("UTF-8", "UTF-8", in, out));
    REQUIRE(out == in);

    REQUIRE(convert_string("ASCII", "ASCII", in, out));
    REQUIRE(out == in);
}

TEST_CASE("UTF-8 and GBK bidirectional conversion", "[gbk]") {
    // "世界你好" in UTF-8
    const std::string utf8_str = "\xe4\xb8\x96\xe7\x95\x8c\xe4\xbd\xa0\xe5\xa5\xbd";
    // "世界你好" in GBK: 世 (0xca 0xc0), 界 (0xbd 0xe7), 你 (0xc4 0xe3), 好 (0xba 0xc3)
    const std::string gbk_str = "\xca\xc0\xbd\xe7\xc4\xe3\xba\xc3";

    SECTION("UTF-8 to GBK") {
        std::string out;
        REQUIRE(convert_string("GBK", "UTF-8", utf8_str, out));
        REQUIRE(out == gbk_str);
    }

    SECTION("GBK to UTF-8") {
        std::string out;
        REQUIRE(convert_string("UTF-8", "GBK", gbk_str, out));
        REQUIRE(out == utf8_str);
    }
}

TEST_CASE("UTF-8 to UTF-16LE and UTF-16BE", "[utf16]") {
    // "A" (0x0041)
    const std::string utf8_a = "A";

    SECTION("UTF-8 to UTF-16LE") {
        std::string out;
        REQUIRE(convert_string("UTF-16LE", "UTF-8", utf8_a, out));
        REQUIRE(out.size() == 2);
        REQUIRE(static_cast<unsigned char>(out[0]) == 0x41);
        REQUIRE(static_cast<unsigned char>(out[1]) == 0x00);
    }

    SECTION("UTF-8 to UTF-16BE") {
        std::string out;
        REQUIRE(convert_string("UTF-16BE", "UTF-8", utf8_a, out));
        REQUIRE(out.size() == 2);
        REQUIRE(static_cast<unsigned char>(out[0]) == 0x00);
        REQUIRE(static_cast<unsigned char>(out[1]) == 0x41);
    }
}

TEST_CASE("Error handling: invalid multibyte sequence", "[errors]") {
    iconv_t cd = iconv_open("UTF-8", "UTF-8");
    REQUIRE(cd != (iconv_t)(-1));

    // 0xFF is an invalid initial byte in UTF-8
    char invalid_bytes[] = "abc\xff" "def";
    char* inbuf = invalid_bytes;
    size_t inbytesleft = 7;

    char out_buffer[32] = {0};
    char* outbuf = out_buffer;
    size_t outbytesleft = sizeof(out_buffer);

    errno = 0;
    size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    REQUIRE(res == (size_t)(-1));
    REQUIRE(errno == EILSEQ);

    iconv_close(cd);
}

TEST_CASE("Error handling: incomplete multibyte sequence", "[errors]") {
    iconv_t cd = iconv_open("UTF-8", "UTF-8");
    REQUIRE(cd != (iconv_t)(-1));

    // First byte of 3-byte Chinese character without following 2 bytes
    char incomplete_bytes[] = "\xe4";
    char* inbuf = incomplete_bytes;
    size_t inbytesleft = 1;

    char out_buffer[32] = {0};
    char* outbuf = out_buffer;
    size_t outbytesleft = sizeof(out_buffer);

    errno = 0;
    size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    REQUIRE(res == (size_t)(-1));
    REQUIRE(errno == EINVAL);

    iconv_close(cd);
}

TEST_CASE("Error handling: output buffer too small (E2BIG)", "[errors]") {
    iconv_t cd = iconv_open("UTF-8", "UTF-8");
    REQUIRE(cd != (iconv_t)(-1));

    char in_str[] = "Hello World!";
    char* inbuf = in_str;
    size_t inbytesleft = strlen(in_str);

    // Provide a buffer of only 2 bytes
    char out_buffer[2] = {0};
    char* outbuf = out_buffer;
    size_t outbytesleft = sizeof(out_buffer);

    errno = 0;
    size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    REQUIRE(res == (size_t)(-1));
    REQUIRE(errno == E2BIG);

    iconv_close(cd);
}

TEST_CASE("Transliteration and Ignore suffixes", "[suffixes]") {
    SECTION("//IGNORE ignores invalid/unrepresentable characters") {
        iconv_t cd = iconv_open("ASCII//IGNORE", "UTF-8");
        REQUIRE(cd != (iconv_t)(-1));

        std::string input = "Hello \xe4\xbd\xa0\xe5\xa5\xbd World"; // "Hello 你好 World"
        char* inbuf = const_cast<char*>(input.data());
        size_t inbytesleft = input.size();

        char out_buffer[64] = {0};
        char* outbuf = out_buffer;
        size_t outbytesleft = sizeof(out_buffer) - 1;

        size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        REQUIRE(res != (size_t)(-1));

        std::string result(out_buffer);
        REQUIRE(result == "Hello  World");

        iconv_close(cd);
    }
}

TEST_CASE("Localcharset query", "[localcharset]") {
    const char* cs = locale_charset();
    REQUIRE(cs != nullptr);
    REQUIRE(std::strlen(cs) > 0);
}
