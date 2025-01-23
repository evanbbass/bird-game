#pragma once

namespace BirdGame
{
	class Window;

	class IRenderer
	{
	public:
		IRenderer() = default;
		virtual ~IRenderer() = default;

		virtual void Initialize(Window& window) = 0;
		virtual void Shutdown() = 0;

		virtual void Render() = 0;

	private:
		IRenderer(const IRenderer&) = delete;
	};
}
