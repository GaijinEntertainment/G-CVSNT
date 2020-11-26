#include <algorithm>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace flags {

using namespace std::string_literals;

namespace detail {

std::vector<std::string> split(std::string s, std::string delim) {
    std::vector<std::string> result;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delim)) != std::string::npos) {
        token = s.substr(0, pos);
        result.push_back(token);
        s.erase(0, pos + delim.length());
    }
    result.push_back(s);
    return result;
}

enum class flag_type {
    optional,
    required,
    flag,
};

} // namespace detail

class flags {
  public:
    flags(int argc, const char** argv) : argc_(argc), argv_(argv) {}

    void info(std::string program_name, std::string description) {
        program_name_ = program_name;
        description_ = description;
    }

    bool passed(std::string flag, std::string det) {
        flags_.emplace_back(flag, detail::flag_type::flag, ""s, det);
        return passed_(flag);
    }

    std::string arg(std::string flag, std::string det) {
        flags_.emplace_back(flag, detail::flag_type::required, ""s, det);
        return arg_(flag);
    }

    std::vector<std::string> args(std::string flag, std::string det) {
        flags_.emplace_back(flag, detail::flag_type::optional, "[]", det);

        auto pos = std::find(argv_, argv_ + argc_, flag);
        std::vector<std::string> result;
        if (pos == argv_ + argc_ || pos + 1 == argv_ + argc_) {
            return result;
        }
        pos++;
        while (pos != argv_ + argc_ && *pos[0] != '-') {
            result.push_back(std::string(*pos));
            pos++;
        }

        return result;
    }

    template <typename T>
    T arg_as(std::string flag, std::string det) {
        flags_.emplace_back(flag, detail::flag_type::required, "", det);

        auto value = std::stringstream(arg_(flag));
        T x = {};
        value >> x;
        return x;
    }

    template <typename T>
    std::vector<T> args_as(std::string flag, std::string det) {
        flags_.emplace_back(flag, detail::flag_type::optional, "[]", det);

        auto parts = detail::split(arg_(flag), ",");
        std::vector<T> xs;
        for (auto part : parts) {
            auto value = std::stringstream(part);
            T x = {};
            value >> x;
            xs.push_back(x);
        }

        return xs;
    }

    template <typename T>
    T arg_as_or(std::string flag, T alt, std::string det) {
        flags_.emplace_back(flag, detail::flag_type::optional,
                            std::to_string(alt), det);

        if (!passed_(flag)) {
            return alt;
        }
        auto value = std::stringstream(arg_(flag));
        T x = {};
        value >> x;
        return x;
    }

    std::string arg_or(std::string flag, std::string alt, std::string det) {
        flags_.emplace_back(flag, detail::flag_type::optional, alt, det);

        if (!passed_(flag)) {
            return alt;
        }
        return arg_(flag);
    }

    bool required_arguments(const std::vector<std::string>& args) {
        for (auto& arg : args) {
            if (!passed_(arg)) {
                return false;
            }
        }
        return true;
    }

    bool sane() {
        for (auto [flag, type, alt, detail] : flags_) {
            (void)alt;
            if (type == detail::flag_type::required && !passed_(flag)) {
                return false;
            }
        }
        return true;
    }

    std::string print_missing() {
        auto output = std::stringstream("");
        for (auto [flag, type, alt, detail] : flags_) {
            (void)alt;
            (void)detail;
            if (type == detail::flag_type::required && !passed_(flag)) {
                output << " " << flag;
            }
        }
        return output.str();
    }

    std::string usage() {
        auto flag_name = [](const std::string &flag, const detail::flag_type type, const std::string &alt) {
            auto output = std::stringstream("");
            output << flag;
            if (type == detail::flag_type::optional)
                output << "[=" << alt << "]" ;
             else if (type == detail::flag_type::flag)
                output << "flag";
            else
                output << "    ";
            return output.str();
        };
        auto flag_name2 = [&](auto& f) {
          return flag_name(std::get<0>(f), std::get<1>(f), std::get<2>(f));
        };
        auto max_flag_size = flag_name2(*std::max_element(
            flags_.begin(), flags_.end(), [&](auto& lhs, auto& rhs) {
                return flag_name2(lhs).size() < flag_name2(rhs).size();
            })).size();
        auto output = std::stringstream("");

        if (program_name_.empty()) {
            program_name_ = argv_[0];
        }
        output << program_name_ << "\n";
        if (!description_.empty()) {
            output << description_ << "\n";
        }
        output << "\n";

        output << "USAGE: " << program_name_ << " [OPTIONS]\n\n";

        output << "OPTIONS:\n";
        for (auto [flag, type, alt, detail] : flags_) {
            output << "  ";
            auto flag_print = flag_name(flag, type, alt);
            output << flag_print;
            auto padding = max_flag_size - flag_print.size();
            output << std::string(padding, ' ');
            if (detail.length()>0)
                output << "| " << detail;

            output << "\n";
        }
        return output.str();
    }

  private:
    bool passed_(std::string flag) {
        return std::find(argv_, argv_ + argc_, flag) != (argv_ + argc_);
    }

    std::string arg_(std::string flag) {
        auto pos = std::find(argv_, argv_ + argc_, flag);
        if (pos == argv_ + argc_ || pos + 1 == argv_ + argc_) {
            return "";
        }
        pos++;
        return std::string(*pos);
    }

    int argc_;
    const char** argv_;
    typedef std::tuple<std::string, detail::flag_type, std::string, std::string> flag_t;
    std::vector<flag_t> flags_;
    std::string program_name_;
    std::string description_;
};

} // namespace flags