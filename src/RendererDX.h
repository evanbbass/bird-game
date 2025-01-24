#pragma once

#include "IRenderer.h"

namespace BirdGame
{
	class RendererImpl;

	class RendererDX final : public IRenderer
	{
	public:
		RendererDX();
		~RendererDX();

		virtual void Initialize(Window& window) override;
		virtual void Shutdown() override;

		virtual void Render() override;

	private:
		RendererDX(const RendererDX&) = delete;

		std::unique_ptr<RendererImpl> mImpl;
	};
}
