// MT25022 - J Chandra Harsha
// Intervention classes implementation

#include "MT25022_Intervention.h"

// =================== MaskIntervention ===================

MaskIntervention::MaskIntervention(double m) : maskFactor_(m) {
    if (m <= 0.0 || m > 1.0) {
        throw std::invalid_argument("MaskIntervention: mask factor must be in (0, 1], got " + std::to_string(m));
    }
}

std::string MaskIntervention::apply(double& beta_eff, double /*infectedFraction*/) {
    beta_eff *= maskFactor_;
    if (maskFactor_ < 1.0) {
        return "Masks applied (factor=" + std::to_string(maskFactor_) + ")";
    }
    return "";
}

std::string MaskIntervention::getName() const { return "Mask"; }

bool MaskIntervention::isActive() const { return maskFactor_ < 1.0; }

double MaskIntervention::getMaskFactor() const { return maskFactor_; }

// =================== LockdownIntervention ===================

LockdownIntervention::LockdownIntervention(double lambda, double thetaOn, double thetaOff)
    : lambda_(lambda), thetaOn_(thetaOn), thetaOff_(thetaOff), inLockdown_(false) {
    if (lambda <= 0.0 || lambda > 1.0) {
        throw std::invalid_argument("LockdownIntervention: lambda must be in (0, 1], got " + std::to_string(lambda));
    }
    if (thetaOff >= thetaOn) {
        throw std::invalid_argument("LockdownIntervention: theta_off must be < theta_on");
    }
    if (thetaOn <= 0.0 || thetaOn >= 1.0) {
        throw std::invalid_argument("LockdownIntervention: theta_on must be in (0, 1)");
    }
    if (thetaOff < 0.0 || thetaOff >= 1.0) {
        throw std::invalid_argument("LockdownIntervention: theta_off must be in [0, 1)");
    }
}

std::string LockdownIntervention::apply(double& beta_eff, double infectedFraction) {
    if (!inLockdown_ && infectedFraction > thetaOn_) {
        inLockdown_ = true;
    } else if (inLockdown_ && infectedFraction < thetaOff_) {
        inLockdown_ = false;
    }
    
    if (inLockdown_) {
        beta_eff *= lambda_;
        return "Lockdown ACTIVE (infected=" + std::to_string(infectedFraction) + ")";
    }
    return "";
}

std::string LockdownIntervention::getName() const { return "Lockdown"; }

bool LockdownIntervention::isActive() const { return inLockdown_; }

double LockdownIntervention::getLambda() const { return lambda_; }
double LockdownIntervention::getThetaOn() const { return thetaOn_; }
double LockdownIntervention::getThetaOff() const { return thetaOff_; }
bool LockdownIntervention::isInLockdown() const { return inLockdown_; }

// =================== InterventionManager ===================

InterventionManager::InterventionManager(double maskFactor, double lambda,
                                         double thetaOn, double thetaOff)
    : mask_(maskFactor), lockdown_(lambda, thetaOn, thetaOff) {}

double InterventionManager::computeBetaEff(double baseBeta, double infectedFraction) {
    double beta_eff = baseBeta;
    mask_.apply(beta_eff, infectedFraction);
    lockdown_.apply(beta_eff, infectedFraction);
    return beta_eff;
}

const MaskIntervention& InterventionManager::getMask() const { return mask_; }
const LockdownIntervention& InterventionManager::getLockdown() const { return lockdown_; }
bool InterventionManager::isLockdownActive() const { return lockdown_.isInLockdown(); }
