#pragma once

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <vector>


namespace utils {
    class invalid_file final : public std::invalid_argument {
    private:
        [[gnu::cold]]
        static std::string message(const std::string& filename, const char *reason) noexcept {
            std::ostringstream buf;
            buf << "File \"" << filename << "\" " << reason << ".";
            return buf.str();
        }

        [[gnu::cold]]
        explicit invalid_file(const std::string& filename, const char *reason):
            std::invalid_argument(message(filename, reason))
        { }

    public:
        [[gnu::cold]]
        static invalid_file is_empty_or_missing(const std::string& filename) {
            return invalid_file(filename, "is empty or missing");
        }

        [[gnu::cold]]
        static invalid_file contains_invalid_data(const std::string& filename) {
            return invalid_file(filename, "contains invalid data");
        }
    };


    class not_enough_items final : public std::out_of_range {
    private:
        [[gnu::cold]]
        static std::string message(const char *type, size_t current, size_t expected) noexcept {
            std::ostringstream buf;
            buf << "Not enough '" << type << "', requesting " << expected << " out of " << current << " available." << '.';
            return buf.str();
        }

        [[gnu::cold]]
        explicit not_enough_items(const char *type, size_t current, size_t expected):
            std::out_of_range(message(type, current, expected))
        { }

    public:
        template<typename Item> [[gnu::cold]]
        static not_enough_items of(size_t current, size_t expected) {
            return not_enough_items(typeid(Item).name(), current, expected);
        }
    };

    using seed_type = std::ranlux48::result_type;

    template<std::ranges::input_range Range> [[gnu::cold]]
    static inline auto sample(Range&& input, size_t count, seed_type seed) {
        using item = std::ranges::range_value_t<Range>;
        if (count > input.size()) [[unlikely]] {
            throw not_enough_items::of<item>(input.size(), count);
        }
        std::vector<item> output;
        output.reserve(count);

        auto rng = std::ranlux48(seed);
        std::ranges::sample(input, std::back_inserter(output), count, rng);
        return output;
    }
}


struct vertex final {
private:
    [[gnu::cold]]
    static inline size_t next_id(void) noexcept {
        static size_t count = 1;
        return count++;
    }

    [[gnu::cold]]
    inline constexpr vertex(size_t id, double x1, double y1, double x2, double y2) noexcept:
        id(id), x1(x1), y1(y1), x2(x2), y2(y2)
    { }

public:
    size_t id;
    double x1, y1;
    double x2, y2;

    [[gnu::cold]]
    vertex(double x1, double y1, double x2, double y2) noexcept:
        vertex(vertex::next_id(), x1, y1, x2, y2)
    { }

    constexpr vertex() noexcept: vertex(SIZE_MAX, 0, 0, 0, 0) {}

    vertex(const vertex& v) noexcept: vertex(v.id, v.x1, v.y1, v.x2, v.y2) { }

    [[gnu::pure]] [[gnu::hot]] [[gnu::nothrow]]
    constexpr inline double cost1(const vertex& other) const noexcept {
        return ceil(hypot(this->x1 - other.x1, this->y1 - other.y1));
    }

    [[gnu::pure]] [[gnu::hot]] [[gnu::nothrow]]
    constexpr inline double cost2(const vertex& other) const noexcept {
        return ceil(hypot(this->x2 - other.x2, this->y2 - other.y2));
    }

    [[gnu::pure]] [[gnu::nothrow]]
    constexpr inline bool operator==(const vertex& other) const noexcept {
        return this->id == other.id;
    }

    [[gnu::nothrow]]
    constexpr inline vertex& operator=(const vertex& other) noexcept {
        this->id = other.id;
        this->x1 = other.x1;
        this->y1 = other.y1;
        this->x2 = other.x2;
        this->y2 = other.y2;
        return *this;
    }

    [[gnu::cold]]
    constexpr static vertex with_id(size_t id, double x1, double y1, double x2, double y2) noexcept {
        return vertex(id, x1, y1, x2, y2);
    }

    [[gnu::cold]]
    static std::vector<vertex> read(const std::string& filename) {
        auto vertices = std::vector<vertex>();
        auto file = std::ifstream(filename, std::ios::in);

        auto line = std::string();
        while (std::getline(file, line)) [[unlikely]] {
            vertex new_vertex;
            if (std::istringstream(line) >> new_vertex) {
                vertices.push_back(new_vertex);
            } else [[unlikely]] {
                throw utils::invalid_file::contains_invalid_data(filename);
            }
        }
        file.close();

        if (vertices.size() <= 0) [[unlikely]] {
            throw utils::invalid_file::is_empty_or_missing(filename);
        }
        return vertices;
    }

    [[gnu::cold]]
    friend inline std::ostream& operator<<(std::ostream& os, const vertex& vertex) {
        return os << "v:" << vertex.id;
    }

    [[gnu::cold]]
    friend inline std::istream& operator>>(std::istream& is, vertex& vertex) {
        return is >> vertex.x1 >> vertex.y1 >> vertex.x2 >> vertex.y2;
    }
};
