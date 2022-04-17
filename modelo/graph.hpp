#pragma once

#include <chrono>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <sstream>
#include <vector>

#include <gurobi_c++.h>
#include "vertex.hpp"
#include "elimination.hpp"


namespace utils {
    [[gnu::cold]]
    static GRBEnv quiet_env(void) {
        auto env = GRBEnv(true);
        env.set(GRB_IntParam_OutputFlag, 0);
        env.set(GRB_IntParam_LazyConstraints, 1);
        env.start();
        return env;
    }

    [[gnu::cold]]
    static std::string join(std::ranges::forward_range auto range, const std::string_view& sep) {
        std::ostringstream buf;
        bool first = true;

        for (const auto& item : range) {
            if (!first) {
                buf << sep;
            }
            buf << item;
            first = false;
        }
        return buf.str();
    }

    class invalid_solution final : public std::domain_error {
    public:
        const std::vector<vertex> vertices;
        const std::optional<tour> subtour;

    private:
        [[gnu::cold]]
        explicit inline invalid_solution(std::vector<vertex> vertices, std::optional<tour> subtour, const char *message):
            std::domain_error(message), vertices(vertices), subtour(subtour)
        { }

    public:
        [[gnu::cold]]
        static invalid_solution zero_solutions(const std::vector<vertex>& vertices) {
            return invalid_solution(vertices, std::nullopt, "No integral solution could be found.");
        }

        [[gnu::cold]]
        static invalid_solution incomplete_tour(const std::vector<vertex>& vertices, tour& subtour) {
            return invalid_solution(vertices, subtour, "Solution found, but leads to incomplete tour.");
        }
    };
}


struct graph final {
private:
    GRBModel model;

    [[gnu::cold]]
    inline GRBVar add_edge(const vertex& u, const vertex& v) {
        std::ostringstream name;
        name << "x_" << u.id() << '_' << v.id();

        double objective = u.cost1(v);
        return this->model.addVar(0., 1., objective, GRB_BINARY, name.str());
    }

    [[gnu::cold]]
    inline auto add_vars(void) {
        utils::matrix<GRBVar> vars(this->order());

        for (unsigned u = 0; u < this->order(); u++) {
            for (unsigned v = u + 1; v < this->order(); v++) {
                auto x_uv = this->add_edge(this->vertices[u], this->vertices[v]);
                vars[u][v] = x_uv;
                vars[v][u] = x_uv;
            }
        }
        return vars;
    }

    [[gnu::cold]]
    inline void add_constraint_deg_2(void) {
        for (unsigned u = 0; u < this->order(); u++) {
            auto expr = GRBLinExpr();
            for (unsigned v = 0; v < this->order(); v++) {
                if (u != v) [[likely]] {
                    expr += this->vars[u][v];
                }
            }
            this->model.addConstr(expr, GRB_EQUAL, 2.);
        }
        this->model.update();
    }

public:
    [[gnu::cold]]
    graph(std::vector<vertex> vertices, const GRBEnv& env):
        model(env), vertices(vertices), vars(this->add_vars())
    {
        this->add_constraint_deg_2();
    }

    const std::vector<vertex> vertices;
    const  utils::matrix<GRBVar> vars;

    /** Number of vertices. */
    [[gnu::pure]] [[gnu::hot]] [[gnu::nothrow]]
    inline size_t order(void) const noexcept {
        return this->vertices.size();
    }

    /** Number of edges. */
    [[gnu::pure]] [[gnu::cold]] [[gnu::nothrow]]
    inline size_t size(void) const noexcept {
        const size_t order = this->order();
        return (order * (order - 1)) / 2;
    }

    using clock = std::chrono::high_resolution_clock;
    const clock::time_point start = clock::now();

    [[gnu::cold]] [[gnu::nothrow]]
    inline double elapsed(void) const noexcept {
        auto end = clock::now();
        std::chrono::duration<double> secs = end - this->start;
        return secs.count();
    }

    [[gnu::pure]] [[gnu::cold]]
    inline int64_t solution_count(void) const {
        return (int64_t) this->model.get(GRB_IntAttr_SolCount);
    }

    [[gnu::hot]]
    double solve(void) {
        auto callback = subtour_elim(this->vertices, this->vars);
        this->model.setCallback(&callback);

        this->model.optimize();
        auto total_time = this->elapsed();

        if (this->solution_count() <= 0) [[unlikely]] {
            throw utils::invalid_solution::zero_solutions(this->vertices);
        }
        return total_time;
    }

    [[gnu::pure]] [[gnu::cold]]
    inline int64_t iterations(void) const {
        return (int64_t) this->model.get(GRB_DoubleAttr_IterCount);
    }

    [[gnu::pure]] [[gnu::cold]]
    inline int64_t var_count(void) const {
        return (int64_t) this->model.get(GRB_IntAttr_NumVars);
    }

    [[gnu::pure]] [[gnu::cold]]
    inline int64_t constr_count(void) const {
        return (int64_t) this->model.get(GRB_IntAttr_NumConstrs);
    }

    [[gnu::pure]] [[gnu::cold]]
    inline auto edges(void) const {
        return utils::get_solutions(this->order(), [this](unsigned i, unsigned j) {
            return this->vars[i][j].get(GRB_DoubleAttr_X) > 0.5;
        });
    }

    [[gnu::pure]] [[gnu::cold]]
    inline auto tour(void) const {
        auto min = utils::min_sub_tour(this->vertices, [this](unsigned i, unsigned j) {
            return this->vars[i][j].get(GRB_DoubleAttr_X) > 0.5;
        });

        if (min.size() != this->order()) [[unlikely]] {
            throw utils::invalid_solution::incomplete_tour(this->vertices, min);
        }
        return min;
    }

    [[gnu::pure]] [[gnu::cold]]
    inline auto solution(void) const {
        const auto tour = this->tour();

        auto vertices = std::vector<vertex>();
        vertices.reserve(tour.size());

        for (unsigned v : tour) {
            vertices.push_back(this->vertices[v]);
        }
        return vertices;
    }

    // inline
};
