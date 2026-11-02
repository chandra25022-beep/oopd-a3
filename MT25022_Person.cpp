// MT25022 - J Chandra Harsha
// Person class implementation

#include "MT25022_Person.h"
#include <sstream>

// =================== Constructors ===================

Person::Person() : state_(State::S), counter_(0) {}

Person::Person(State s, int count) : state_(s), counter_(count) {
    if (count < 0) {
        throw std::invalid_argument("Person: infection counter cannot be negative (got " + std::to_string(count) + ")");
    }
    if (s != State::I && count != 0) {
        counter_ = 0; // counter only meaningful for Infected
    }
}

// =================== Accessors ===================

State Person::getState() const { return state_; }
int Person::getCounter() const { return counter_; }
bool Person::isInfected() const { return state_ == State::I; }
bool Person::isSusceptible() const { return state_ == State::S; }
bool Person::isAlive() const { return state_ != State::D; }

// =================== Mutators ===================

void Person::setState(State s) {
    state_ = s;
    if (s != State::I) {
        counter_ = 0;
    }
}

void Person::setCounter(int c) {
    if (c < 0) {
        throw std::invalid_argument("Person: counter cannot be negative");
    }
    counter_ = c;
}

void Person::infect() {
    if (state_ != State::S) {
        throw std::logic_error("Person: can only infect a Susceptible person (current state: " + stateToString(state_) + ")");
    }
    state_ = State::I;
    counter_ = 0;
}

void Person::recover() {
    if (state_ != State::I) {
        throw std::logic_error("Person: can only recover an Infected person");
    }
    state_ = State::R;
    counter_ = 0;
}

void Person::die() {
    if (state_ != State::I) {
        throw std::logic_error("Person: can only kill an Infected person");
    }
    state_ = State::D;
    counter_ = 0;
}

void Person::vaccinate() {
    if (state_ != State::S) {
        throw std::logic_error("Person: can only vaccinate a Susceptible person");
    }
    state_ = State::V;
    counter_ = 0;
}

void Person::incrementCounter() {
    if (state_ != State::I) {
        throw std::logic_error("Person: can only increment counter of an Infected person");
    }
    counter_++;
}

// =================== Operators ===================

bool Person::operator==(const Person& other) const {
    return state_ == other.state_ && counter_ == other.counter_;
}

bool Person::operator!=(const Person& other) const {
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, const Person& p) {
    os << Person::stateToString(p.state_);
    if (p.state_ == State::I) {
        os << "(" << p.counter_ << ")";
    }
    return os;
}

// =================== Utility ===================

std::string Person::stateToString(State s) {
    switch (s) {
        case State::S: return "S";
        case State::I: return "I";
        case State::R: return "R";
        case State::V: return "V";
        case State::D: return "D";
        default: return "?";
    }
}

char Person::stateToChar(State s) {
    switch (s) {
        case State::S: return 'S';
        case State::I: return 'I';
        case State::R: return 'R';
        case State::V: return 'V';
        case State::D: return 'D';
        default: return '?';
    }
}

uint64_t Person::hashValue() const {
    uint64_t h = static_cast<uint64_t>(state_) * 31 + static_cast<uint64_t>(counter_);
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
    return h ^ (h >> 31);
}
