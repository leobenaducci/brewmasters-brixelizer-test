#pragma once
#include <iostream>
#include <vector>
#include <string>

class Pass {
public:
    Pass(std::string name) : m_Name(name) {}

    Pass& ReadsFrom(std::string resource) { 
        m_Inputs.push_back(resource); 
        return *this;
    }

    Pass& WritesTo(std::string resource) {
        m_Outputs.push_back(resource); 
        return *this;
    }

    void Execute() {
        std::cout << "[Executing Pass: " << m_Name << "]" << '\n';
    }

    // Necesitaremos acceder a estos datos para el grafo
    std::vector<std::string> const& GetInputs() const noexcept { return m_Inputs; }
    std::vector<std::string> const& GetOutputs() const noexcept { return m_Outputs; }
    std::string const& GetName() const noexcept { return m_Name; }
    
private:
    std::string m_Name {};
    std::vector<std::string> m_Inputs {};
    std::vector<std::string> m_Outputs {};
};