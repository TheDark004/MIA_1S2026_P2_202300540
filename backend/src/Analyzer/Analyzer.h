#pragma once
#include <string>

// ANALYZER — Parsea texto y despacha al módulo correcto
//
// Entrada: string con el comando completo
//   "mkdisk -size=3 -unit=m -fit=bf -path=/home/discos/A.mia"
// Salida: string con el resultado (va al textarea del frontend)
// Funciona con comandos individuales y con scripts .smia completos

namespace Analyzer {

// Analiza UN solo comando y retorna el output
std::string Analyze(const std::string& input);

// Analiza un script .smia completo (varias líneas)
// Retorna todo el output acumulado
std::string AnalyzeScript(const std::string& script);

} 