/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/
#include "CLSHPlasticModel.h"

#include "SymmIsotropicElasticityTensor.h"
#include <cmath>

template <>
InputParameters
validParams<CLSHPlasticModel>()
{
  InputParameters params = validParams<ReturnMappingModel>();
  params.addRequiredParam<Real>("yield_stress",
                                "The point at which plastic strain begins accumulating");
  params.addRequiredParam<Real>("hardening_constant", "Hardening slope");
  params.addRequiredParam<Real>("c_alpha", "creep constant");
  params.addRequiredParam<Real>("c_beta", "creep constant");
  return params;
}

CLSHPlasticModel::CLSHPlasticModel(const InputParameters & parameters)
  : ReturnMappingModel(parameters, "plastic"),
    _yield_stress(parameters.get<Real>("yield_stress")),
    _hardening_constant(parameters.get<Real>("hardening_constant")),
    _c_alpha(parameters.get<Real>("c_alpha")),
    _c_beta(parameters.get<Real>("c_beta")),
    _hardening_variable(declareProperty<Real>("hardening_variable")),
    _hardening_variable_old(getMaterialPropertyOld<Real>("hardening_variable")),
    _plastic_strain(declareProperty<SymmTensor>("plastic_strain")),
    _plastic_strain_old(getMaterialPropertyOld<SymmTensor>("plastic_strain"))
{
}

void
CLSHPlasticModel::initQpStatefulProperties()
{
  _hardening_variable[_qp] = 0;
  ReturnMappingModel::initQpStatefulProperties();
}

void
CLSHPlasticModel::computeStressInitialize(Real effectiveTrialStress,
                                          const SymmElasticityTensor & elasticityTensor)
{
  const SymmIsotropicElasticityTensor * eT =
      dynamic_cast<const SymmIsotropicElasticityTensor *>(&elasticityTensor);
  if (!eT)
  {
    mooseError("CLSHPlasticModel requires a SymmIsotropicElasticityTensor");
  }
  _shear_modulus = eT->shearModulus();
  _yield_condition = effectiveTrialStress - _hardening_variable_old[_qp] - _yield_stress;
  _hardening_variable[_qp] = _hardening_variable_old[_qp];
  _plastic_strain[_qp] = _plastic_strain_old[_qp];
}

Real
CLSHPlasticModel::computeResidual(const Real effectiveTrialStress, const Real scalar)
{
  Real residual = 0.0;
  if (_yield_condition > 0)
  {
    const Real xflow = _c_beta * (effectiveTrialStress - (3. * _shear_modulus * scalar) -
                                  _hardening_variable[_qp] - _yield_stress);
    Real xphi = _c_alpha * std::sinh(xflow);
    _xphidp = -3. * _shear_modulus * _c_alpha * _c_beta * std::cosh(xflow);
    _xphir = -_c_alpha * _c_beta * std::cosh(xflow);
    if (_legacy_return_mapping)
      residual = xphi - scalar / _dt;
    else
      residual = xphi * _dt - scalar;
  }

  return residual;
}

void
CLSHPlasticModel::iterationFinalize(Real scalar)
{
  _hardening_variable[_qp] = _hardening_variable_old[_qp] + (_hardening_constant * scalar);
}

Real
CLSHPlasticModel::computeDerivative(const Real /*effectiveTrialStress*/, const Real /*scalar*/)
{
  Real derivative = 1.0;
  if (_yield_condition > 0)
  {
    if (_legacy_return_mapping)
      derivative = _xphidp + _hardening_constant * _xphir - 1.0 / _dt;
    else
      derivative = _xphidp * _dt + _hardening_constant * _xphir * _dt - 1.0;
  }

  return derivative;
}

void
CLSHPlasticModel::computeStressFinalize(const SymmTensor & plasticStrainIncrement)
{
  _plastic_strain[_qp] += plasticStrainIncrement;
}
