#include "riscv_cv.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Read an 8-bit binary RGB PPM (P6) image.
bool ReadPpm(const char* path, int& width, int& height,
             std::vector<std::uint8_t>& rgb)
{
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }

    char magic[3] = {};
    int max_value = 0;
    const bool valid_header =
        std::fscanf(file, "%2s %d %d %d", magic, &width, &height, &max_value) == 4 &&
        std::strcmp(magic, "P6") == 0 && width > 0 && height > 0 &&
        max_value == 255;

    const int separator = std::fgetc(file);
    if (!valid_header || separator == EOF ||
        !std::isspace(static_cast<unsigned char>(separator)))
    {
        std::fclose(file);
        return false;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    rgb.resize(3 * pixel_count);
    const bool complete = std::fread(rgb.data(), 1, rgb.size(), file) == rgb.size();
    std::fclose(file);
    return complete;
}

// Write an 8-bit binary grayscale PGM (P5) without replacing an existing file.
bool WritePgm(const char* path, int width, int height,
              const std::vector<std::uint8_t>& gray)
{
    std::FILE* existing = std::fopen(path, "rb");
    if (existing != nullptr)
    {
        std::fclose(existing);
        std::printf("Output already exists; kept %s unchanged.\n", path);
        return true;
    }

    std::FILE* file = std::fopen(path, "wbx");
    if (file == nullptr)
    {
        return false;
    }

    const bool complete =
        std::fprintf(file, "P5\n%d %d\n255\n", width, height) > 0 &&
        std::fwrite(gray.data(), 1, gray.size(), file) == gray.size();
    const bool closed = std::fclose(file) == 0;
    if (complete && closed)
    {
        std::printf("Wrote %dx%d grayscale image to %s\n", width, height, path);
    }
    return complete && closed;
}

int main(int argc, char* argv[])
{
    if (argc > 3)
    {
        std::fprintf(stderr, "Usage: %s [input.ppm [output.pgm]]\n", argv[0]);
        return 1;
    }

    // The source file is in example/source.
    std::string project_dir = __FILE__;
    project_dir.resize(project_dir.find_last_of('/'));
    project_dir.resize(project_dir.find_last_of('/'));
    project_dir.resize(project_dir.find_last_of('/'));
    const std::string default_input = project_dir + "/images/sample_640×426.ppm";
    const std::string default_output = project_dir + "/images/RgbToGray_ppm_output.pgm";

    const char* input_path = argc >= 2 ? argv[1] : default_input.c_str();
    const char* output_path = argc == 3 ? argv[2] : default_output.c_str();

    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
    if (!ReadPpm(input_path, width, height, rgb))
    {
        std::fprintf(stderr, "Could not read P6 PPM: %s\n", input_path);
        return 1;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> gray(pixel_count);
    vec::RgbToGray(rgb.data(), gray.data(), pixel_count);

    if (!WritePgm(output_path, width, height, gray))
    {
        std::fprintf(stderr, "Could not write P5 PGM: %s\n", output_path);
        return 1;
    }

    std::printf("Example passed.\n");
    return 0;
}
