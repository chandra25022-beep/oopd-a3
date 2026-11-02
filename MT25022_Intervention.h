// MT25022 - J Chandra Harsha
// Intervention classes for modifying transmission probability
// Demonstrates inheritance and polymorphism

#ifndef MT25022_INTERVENTION_H
#define MT25022_INTERVENTION_H

#include <string>
#include <stdexcept>

/**
 * @brief Abstract base class for interventions that modify beta_eff.
 * Demonstrates polymorphism through virtual functions.
 */
class Intervention {
public:
    virtual ~Intervention() = default;
    
    /**
     * @brief Apply this intervention to modify beta_eff.
     * @param beta_eff    Current effective transmission probability (modified in-place)
     * @param infectedFraction Current fraction of population that is infected
     * @return std::string Description of what was applied (for logging)
     */
    virtual std::string apply(double& beta_eff, double infectedFraction) = 0;
    
    /** @brief Get the name of this intervention. */
    virtual std::string getName() const = 0;
    
    /** @brief Check if intervention is currently active. */
    virtual bool isActive() const = 0;
};

/**
 * @brief Mask intervention: multiplies beta by a mask factor m.
 * When m = 1, masks have no effect. When m < 1, transmission is reduced.
 */
class MaskIntervention : public Intervention {
private:
    double maskFactor_;   ///< Mask reduction factor (0 < m <= 1)
    
public:
    /**
     * @brief Construct a mask intervention.
     * @param m Mask factor (0 < m <= 1)
     * @throws std::invalid_argument if m is out of range
     */
    explicit MaskIntervention(double m = 1.0);
    
    std::string apply(double& beta_eff, double infectedFraction) override;
    std::string getName() const override;
    bool isActive() const override;
    
    double getMaskFactor() const;
};

/**
 * @brief Lockdown intervention: multiplies beta by lambda when infected fraction
 * rises above theta_on, and lifts when it falls below theta_off.
 */
class LockdownIntervention : public Intervention {
private:
    double lambda_;      ///< Lockdown reduction factor
    double thetaOn_;     ///< Threshold to trigger lockdown
    double thetaOff_;    ///< Threshold to lift lockdown
    bool inLockdown_;    ///< Whether lockdown is currently active
    
public:
    /**
     * @brief Construct a lockdown intervention.
     * @param lambda  Lockdown factor
     * @param thetaOn  Threshold to enter lockdown
     * @param thetaOff Threshold to exit lockdown
     * @throws std::invalid_argument if thresholds are invalid
     */
    LockdownIntervention(double lambda = 0.3, double thetaOn = 0.02, double thetaOff = 0.005);
    
    std::string apply(double& beta_eff, double infectedFraction) override;
    std::string getName() const override;
    bool isActive() const override;
    
    double getLambda() const;
    double getThetaOn() const;
    double getThetaOff() const;
    bool isInLockdown() const;
};

/**
 * @brief Combined intervention manager that composes multiple interventions.
 * Demonstrates composition pattern.
 */
class InterventionManager {
private:
    MaskIntervention mask_;
    LockdownIntervention lockdown_;
    
public:
    /**
     * @brief Construct with mask and lockdown parameters.
     */
    InterventionManager(double maskFactor = 1.0, 
                        double lambda = 0.3,
                        double thetaOn = 0.02, 
                        double thetaOff = 0.005);
    
    /**
     * @brief Compute effective beta for the next day.
     * @param baseBeta          Base transmission probability
     * @param infectedFraction  Current infected fraction
     * @return double           Effective beta after all interventions
     */
    double computeBetaEff(double baseBeta, double infectedFraction);
    
    /** @brief Get reference to mask intervention. */
    const MaskIntervention& getMask() const;
    
    /** @brief Get reference to lockdown intervention. */
    const LockdownIntervention& getLockdown() const;
    
    /** @brief Check if lockdown is active. */
    bool isLockdownActive() const;
};

#endif // MT25022_INTERVENTION_H
