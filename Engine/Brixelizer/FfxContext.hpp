#pragma once

#include <FidelityFX/host/backends/dx12/ffx_dx12.h>

#include <cassert>
#include <vector>

namespace Brixelizer {

    class FfxContext {
    public:
        FfxContext() = default;
        ~FfxContext() = default;

        bool Initialize(ID3D12Device* device, ID3D12CommandQueue* cmdQueue) noexcept {
            size_t scratchSize = ffxGetScratchMemorySizeDX12(2);
            m_ScratchMemory.resize(scratchSize);

            FfxErrorCode err = ffxGetInterfaceDX12(
                &m_FfxInterface,
                device,
                m_ScratchMemory.data(),
                scratchSize,
                2
            );

            return err == FFX_OK;
        }

        FfxContext(const FfxContext&) = delete;
        FfxContext& operator=(const FfxContext&) = delete;
    
        FfxInterface& GetFfxInterface() noexcept { return m_FfxInterface; }

    private:
        FfxInterface m_FfxInterface   {};
        bool m_ContextCreated {};

        std::vector<uint8_t> m_ScratchMemory {};
    };

} // namespace Brixelizer