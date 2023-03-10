#include "pch.h"
#include "ShadedEffect.h"
#include "Texture.h"

ShadedEffect::ShadedEffect(ID3D11Device* pDevice, const std::wstring& assetFile)
	:Effect(pDevice, assetFile)
{
	m_pNormalMapVariable = m_pEffect->GetVariableByName("gNormalMap")->AsShaderResource();
	if (!m_pNormalMapVariable->IsValid())
	{
		std::cout << "m_pNormalMapVariable not valid!\n";
	}

	m_pSpecularMapVariable = m_pEffect->GetVariableByName("gSpecularMap")->AsShaderResource();
	if (!m_pSpecularMapVariable->IsValid())
	{
		std::cout << "m_pSpecularMapVariable not valid!\n";
	}

	m_pGlossinessMapVariable = m_pEffect->GetVariableByName("gGlossinessMap")->AsShaderResource();
	if (!m_pGlossinessMapVariable->IsValid())
	{
		std::cout << "m_pGlossinessMapVariable not valid!\n";
	}
}

ShadedEffect::~ShadedEffect()
{
	if (m_pEffect)
	{
		m_pEffect->Release();
		m_pEffect = nullptr;
	}
}

void ShadedEffect::SetNormalMap(Texture* pNormalTexture) const
{
	if (m_pNormalMapVariable)
	{
		m_pNormalMapVariable->SetResource(pNormalTexture->GetSRV());
	}
}

void ShadedEffect::SetSpecularMap(Texture* pSpecularTexture) const
{
	if (m_pSpecularMapVariable)
	{
		m_pSpecularMapVariable->SetResource(pSpecularTexture->GetSRV());
	}
}

void ShadedEffect::SetGlossinessMap(Texture* pGlossinessTexture) const
{
	if (m_pGlossinessMapVariable)
	{
		m_pGlossinessMapVariable->SetResource(pGlossinessTexture->GetSRV());
	}
}
