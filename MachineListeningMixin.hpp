#pragma once
#include "XiasriAnalysis.hpp"
#include "src/memllib/utils/sharedMem.hpp"
#include "src/memllib/examples/InterfaceRL.hpp"
#include "src/memllib/PicoDefs.hpp"

class MachineListeningMixin {
    XiasriAnalysis mlAnalysis_{48000.f};
    SharedBuffer<float, XiasriAnalysis::kN_Params> machine_list_buffer_;
    InterfaceRLBase* iface_ = nullptr;
public:
    // Call from setupAudio() after audio app Setup() (maxiSettings must be ready)
    void setup(InterfaceRLBase& iface) {
        iface_ = &iface;
        iface.setHasMachineListening(true);
        // Labels for the NN Inputs screen, in XiasriAnalysis::parameters_t order.
        iface.setMLInputLabels({"Ptch", "Aper", "Enrg", "Atk", "Brt", "EnCr"});
        mlAnalysis_.ReinitFilters();
    }
    __force_inline void analyse(stereosample_t x) {
        if (!iface_) return;
        auto src = iface_->getInputSource();
        if (src != InterfaceRLBase::INPUT_SOURCE::MACHINE_LISTENING &&
            src != InterfaceRLBase::INPUT_SOURCE::COMBINED) return;
        union {
            XiasriAnalysis::parameters_t p;
            float v[XiasriAnalysis::kN_Params];
        } u;
        u.p = mlAnalysis_.Process(x.L + x.R);
        machine_list_buffer_.writeNonBlocking(u.v, XiasriAnalysis::kN_Params);
    }
    __force_inline void processAnalysisParams() {
        std::vector<float> mlist_params(XiasriAnalysis::kN_Params, 0);
        machine_list_buffer_.readNonBlocking(mlist_params);
        if (iface_) iface_->readAnalysisParameters(mlist_params);
    }
};
