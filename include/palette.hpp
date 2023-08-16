#ifndef PALETTE_HPP
#define PALETTE_HPP

#include <vector>
#include <array>
#include <fstream>

struct Color
{
    float r, g, b;
    
    constexpr auto &operator[](size_t idx) { return ((float *)(void *)this)[idx]; }
    constexpr const auto &operator[](size_t idx) const { return ((float *)(void *)this)[idx]; }

    bool operator==(const Color &other) { return this->r == other.r && this->g == other.g && this->b == other.b; }
    bool operator!=(const Color &other) { return !(*this == other); }
};

class Palette
{
public:
    Palette();
    ~Palette() = default;

    void LoadFromFile(std::ifstream &stream);
    void SaveToFile(std::ofstream &stream);

    constexpr Color &operator[](size_t idx) { return m_Colors[idx]; }
    constexpr const Color &operator[](size_t idx) const { return m_Colors[idx]; }

    constexpr size_t size(void) { return m_Colors.size(); }
    constexpr void resize(int size) { m_Colors.resize(size); }

    constexpr void clear() { m_Colors.clear(); }
private:
    std::vector<Color> m_Colors;
};

#endif // PALETTE_HPP