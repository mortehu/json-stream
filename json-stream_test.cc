#include <memory>
#include <string>
#include <vector>

#include "third_party/gtest/gtest.h"
#include "json-stream.h"

struct JsonStreamTest : public testing::Test {};

namespace {

struct StreamData {
  std::vector<std::tuple<enum json_stream_event, const char *,
                         union json_stream_value>> events;
  std::vector<std::string> strings;
};

int CollectStream(enum json_stream_event type, const char *name,
                  const union json_stream_value *value, void *user) {
  if (type == JSON_END) return 0;

  auto data = reinterpret_cast<StreamData *>(user);

  if (name) {
    data->strings.emplace_back(name);
    name = data->strings.back().c_str();
  }

  union json_stream_value value_copy = *value;

  if (type == JSON_STRING) {
    data->strings.emplace_back(value->string);
    value_copy.string = const_cast<char *>(data->strings.back().c_str());
  }

  data->events.emplace_back(type, name, value_copy);

  return 0;
}

}  // namespace

TEST_F(JsonStreamTest, ParseString) {
  static const struct {
    const char *json;
    const char *output;
  } kTestCases[] = {
      {"\"hello, world\"", "hello, world"},
      {"\"\\n\\t\\r\\\"\"", "\n\t\r\""},
      {"\"\\u0041\"", "A"},
  };

  for (const auto &test_case : kTestCases) {
    StreamData result;

    std::unique_ptr<json_stream, decltype(&json_stream_end)> stream(
        json_stream_init(CollectStream, &result), json_stream_end);

    std::string json = test_case.json;

    EXPECT_EQ(json.size(),
              json_stream_consume(stream.get(), json.data(), json.size()));

    stream.reset();

    ASSERT_EQ(1, result.events.size());
    ASSERT_EQ(JSON_STRING, std::get<enum json_stream_event>(result.events[0]));
    EXPECT_EQ(nullptr, std::get<const char *>(result.events[0]));
    EXPECT_EQ(
        0, strcmp(test_case.output,
                  std::get<union json_stream_value>(result.events[0]).string))
        << test_case.json << "\n" << test_case.output;
  }
}
