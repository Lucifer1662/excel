
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

class Value {
    std::variant<double, std::string, bool> value;

   public:
    Value(double d) : value(d) {}
    Value(const std::string& s) : value(s) {}
    Value(std::string&& s) : value(std::move(s)) {}
    Value(bool b) : value(b) {}
    Value() : value("") {}
    Value(const Value&) = default;
    Value(Value&&) = default;
    Value& operator=(const Value&) = default;

    std::string toString() const {
        if (value.index() == 0) {
            return std::to_string(std::get<double>(value));
        }
        if (value.index() == 1) {
            return std::get<std::string>(value);
        }
        if (value.index() == 2) {
            return std::get<bool>(value) ? "TRUE" : "FALSE";
        }
        return "";
    }

    template <typename T>
    void update(const T&& t) {
        value.emplace(std::forward<T>(t));
    }

    Value operator+(const Value& rhs) const {
        if (value.index() == 0 && rhs.value.index() == 0) {
            return std::get<double>(value) + std::get<double>(rhs.value);
        }
        if (value.index() == 1 && rhs.value.index() == 1) {
            return std::get<std::string>(value) +
                   std::get<std::string>(rhs.value);
        }
        return *this;
    }
};

struct Expression {
    virtual Value eval() = 0;
    virtual void dirty(){};
    virtual ~Expression() = default;
};

class Constant : public Expression {
    Value value;

   public:
    Constant() : value("") {}

    Constant(Value value) : value(value) {}

    virtual Value eval() { return value; };

    void setValue(Value value) { this->value = value; }
    virtual ~Constant() = default;
};

class Addition : public Expression {
    Expression* l;
    Expression* r;

   public:
    Addition(Expression* l, Expression* r) : l(l), r(r) {}

    virtual Value eval() { return l->eval() + r->eval(); };

    void setLeft(Expression* value) { this->l = value; }
    void setRight(Expression* value) { this->l = value; }
    virtual ~Addition() = default;
};

class UnaryExpression : public Expression {
   protected:
    Expression* expression;

   public:
    UnaryExpression(Expression* expression) : expression(expression) {}
};

class ExpressionCache : public UnaryExpression {
    Value cachedValue;
    bool isDirty = true;

    virtual Value eval() {
        if (isDirty) {
            cachedValue = expression->eval();
        }
        return cachedValue;
    };

    virtual void dirty(){
        isDirty = true;
    }
};

using EId = std::list<std::unique_ptr<Expression*>>::iterator;

namespace std {

template <>
struct hash<EId> {
    std::size_t operator()(const EId& k) const {
        using std::hash;
        using std::size_t;
        using std::string;

        // Compute individual hash values for first,
        // second and third and combine them using XOR
        // and bit shifting:

        return hash<long>()((long)(**k));
    }
};

}  // namespace std

class ExpressionManager {
    std::list<std::unique_ptr<Expression*>> expressions;

   public:
    EId add(Expression* e) {
        expressions.push_back(std::make_unique<Expression*>(e));
        return std::prev(expressions.end());
    }

    void remove(EId id) { expressions.erase(id); }
};

class Dependencies {
    std::unordered_multimap<EId, EId> edges;
    std::unordered_multimap<EId, EId> edgesBack;

   public:
    void add(EId from, EId to) {
        edgesBack.insert({from, to});
        edges.insert({from, to});
    }

    void remove(EId from, EId to) {
        {
            auto [it, end] = edges.equal_range(from);
            for (; it != end; it++) {
                if (it->second == to) {
                    // does not invalidate other iterators, allowed
                    edges.erase(it);
                }
            }
        }

        {
            auto [it, end] = edgesBack.equal_range(to);
            for (; it != end; it++) {
                if (it->second == from) {
                    // does not invalidate other iterators, allowed
                    edgesBack.erase(it);
                }
            }
        }
    }

    auto begin(EId from) {
        auto [begin, end] = edges.equal_range(from);
        return begin;
    }

    auto end(EId from) {
        auto [begin, end] = edges.equal_range(from);
        return end;
    }

    std::unordered_multimap<EId, EId>::iterator beginBack(EId from) {
        auto [begin, end] = edgesBack.equal_range(from);
        return begin;
    }

    std::unordered_multimap<EId, EId>::iterator endBack(EId from) {
        auto [begin, end] = edgesBack.equal_range(from);
        return end;
    }
};

class DirtyFinder {
    Dependencies& dependencies;
    std::unordered_set<EId> dirty;

   public:
    DirtyFinder(Dependencies& dependencies)
        : dependencies(dependencies), dirty({}) {}

    template <typename It>
    void Eval(It begin, It end) {
        std::for_each(begin, end, [this](auto it) { this->Eval(*it); });
    }

    void Eval(EId id) {
        if (dirty.find(id) == dirty.end()) {
            return;
        }
        dirty.insert(id);

        auto begin = dependencies.beginBack(id);
        auto end = dependencies.begin(id);

        std::for_each(begin, end, [this](auto& it) {
            // this->Eval(it->second);
        });
    }

    auto begin() { return dirty.begin(); }

    auto end() { return dirty.end(); }
};

class EvalChange {
    Dependencies& dependencies;

   public:
    EvalChange(Dependencies& dependencies) : dependencies(dependencies) {}

   public:
    template <typename It>
    void change(It begin, It end) {
        DirtyFinder finder(dependencies);
        finder.Eval(begin, end);

        std::for_each(finder.begin(), finder.end(),
                      [this](decltype(finder.begin()) it) { (***it)->eval(); });
    }
};

int main() {
    Dependencies dependencies;
    EvalChange evalChange(dependencies);
    ExpressionManager eMananger;

    auto id1 = eMananger.add(new Constant(Value(5.0)));
    auto id2 = eMananger.add(new Constant(Value(3.0)));
    auto id3 = eMananger.add(new Addition(**id1, **id2));
    dependencies.add(id3, id1);
    dependencies.add(id3, id2);

    std::cout << (**id3)->eval().toString() << std::endl;
}
