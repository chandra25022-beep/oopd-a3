// MT25022 - J Chandra Harsha
// Person class representing an individual in the pandemic simulation grid

#ifndef MT25022_PERSON_H
#define MT25022_PERSON_H

#include <string>
#include <stdexcept>
#include <iostream>
#include <cstdint>

/**
 * @brief Enumeration of possible states for a person.
 */
enum class State { S, I, R, V, D };

/**
 * @brief Represents a single person in the simulation grid.
 * 
 * Each person has a state (S, I, R, V, D) and, if infected,
 * a counter tracking the number of days they have been infected.
 * Demonstrates encapsulation, validation, and operator overloading.
 */
class Person {
private:
    State state_;     ///< Current state
    int counter_;     ///< Days infected (only meaningful when state_ == I)

public:
    // =================== Constructors ===================
    
    /** @brief Default constructor: creates a Susceptible person. */
    Person();
    
    /** 
     * @brief Parameterized constructor.
     * @param s     Initial state
     * @param count Initial infection counter (must be >= 0)
     * @throws std::invalid_argument if counter is negative
     */
    Person(State s, int count = 0);
    
    /** @brief Copy constructor. */
    Person(const Person& other) = default;
    
    /** @brief Move constructor. */
    Person(Person&& other) noexcept = default;
    
    /** @brief Copy assignment. */
    Person& operator=(const Person& other) = default;
    
    /** @brief Move assignment. */
    Person& operator=(Person&& other) noexcept = default;
    
    /** @brief Destructor. */
    ~Person() = default;

    // =================== Accessors ===================
    
    /** @brief Get the current state. */
    State getState() const;
    
    /** @brief Get the infection counter. */
    int getCounter() const;
    
    /** @brief Check if the person is infected. */
    bool isInfected() const;
    
    /** @brief Check if the person is susceptible. */
    bool isSusceptible() const;
    
    /** @brief Check if the person is alive. */
    bool isAlive() const;

    // =================== Mutators ===================
    
    /** 
     * @brief Set the state.
     * @param s New state
     * @throws std::logic_error if transitioning Dead to another state
     */
    void setState(State s);
    
    /** 
     * @brief Set the infection counter.
     * @param c New counter value (must be >= 0)
     * @throws std::invalid_argument if counter is negative
     */
    void setCounter(int c);
    
    /**
     * @brief Infect this person (set state to I, counter to 0).
     * @throws std::logic_error if person is not Susceptible
     */
    void infect();
    
    /**
     * @brief Recover this person (set state to R).
     * @throws std::logic_error if person is not Infected
     */
    void recover();
    
    /**
     * @brief Kill this person (set state to D).
     * @throws std::logic_error if person is not Infected
     */
    void die();
    
    /**
     * @brief Vaccinate this person (set state to V).
     * @throws std::logic_error if person is not Susceptible
     */
    void vaccinate();
    
    /**
     * @brief Increment the infection counter.
     * @throws std::logic_error if person is not Infected
     */
    void incrementCounter();

    // =================== Operators ===================
    
    /** @brief Equality operator. */
    bool operator==(const Person& other) const;
    
    /** @brief Inequality operator. */
    bool operator!=(const Person& other) const;
    
    /** @brief Stream output operator. */
    friend std::ostream& operator<<(std::ostream& os, const Person& p);

    // =================== Utility ===================
    
    /** @brief Get string representation of the state. */
    static std::string stateToString(State s);
    
    /** @brief Get a character code for the state (for PPM coloring). */
    static char stateToChar(State s);
    
    /** 
     * @brief Hash value for checksum computation.
     * @return uint64_t hash combining state and counter
     */
    uint64_t hashValue() const;
};

#endif // MT25022_PERSON_H
