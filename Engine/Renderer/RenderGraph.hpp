#pragma once
#include <unordered_map>
#include <imgui.h>

#include "Pass.hpp"

class RenderGraph {
public:
    struct GraphError {
        std::string passName {};
        std::string resourceName {};
        std::string message {};
    };

    void AddPass(Pass const& pass) {
        m_Passes.push_back(pass);
    }

    void Build() {
        m_ResourceProducer.clear();
        
        for (size_t i{}; i < m_Passes.size(); i++) {
            std::vector<std::string> const& outputs = m_Passes[i].GetOutputs();

            for (std::string const& resourceName : outputs) {
                m_ResourceProducer[resourceName] = i;
            }
        }

        for (const auto& pass : m_Passes) {
            for (const auto& in : pass.GetInputs()) {
                if (m_ResourceProducer.find(in) == m_ResourceProducer.end()) {
                    m_Errors.push_back({pass.GetName(), in, "No hay productor para este recurso"});
                }
            }
        }
    }

    // Aquí es donde sucede la magia después
    void Execute() {
        // Por ahora, solo los ejecutamos en orden de llegada
        for (auto& pass : m_Passes) {
            pass.Execute();
        }
    }

    void RenderUI() {
        ImGui::Begin("Render Graph Visualizer");

        if (ImGui::Button("Recompilar Grafo")) {
            Build();
        }

        ImGui::Separator();

        for (const auto& pass : m_Passes) {
            // Usamos un ID único para ImGui basado en el nombre del pase
            if (ImGui::TreeNode(pass.GetName().c_str())) {
                
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Inputs (Consume):");
                if (pass.GetInputs().empty()) {
                    ImGui::TextDisabled("  (Ninguno)");
                } else {
                    for (const auto& in : pass.GetInputs()) {
                        ImGui::BulletText("%s", in.c_str());
                        // Aquí podríamos buscar quién es el productor y mostrarlo
                        if (m_ResourceProducer.count(in)) {
                            ImGui::SameLine();
                            ImGui::TextDisabled(" <- [Pase %zu]", m_ResourceProducer[in]);
                        }
                    }
                }

                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Outputs (Produce):");
                for (const auto& out : pass.GetOutputs()) {
                    ImGui::BulletText("%s", out.c_str());
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();
    }

private:
    std::vector<GraphError> m_Errors {};
    std::vector<Pass> m_Passes {};
    std::unordered_map<std::string, size_t> m_ResourceProducer {};
};