#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <unordered_set>
#include <cstring>
#include <stdexcept>

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

struct Image {
    BMPHeader header;
    std::vector<uint8_t> data;
    std::vector<uint8_t> colorTable;
};

Image readBMP(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << strerror(errno) << "\n";
        throw std::runtime_error("Error opening file");
    }
    BMPHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(BMPHeader));

    if (header.bfType != 0x4D42) {
        std::cerr << "Not a BMP file: " << filename << "\n";
        throw std::runtime_error("Not a BMP file");
    }

    std::vector<uint8_t> colorTable;
    if (header.biBitCount == 8) {
        colorTable.resize(256 * 4);
        file.read(reinterpret_cast<char*>(colorTable.data()), colorTable.size());
    }

    header.biSizeImage = (uint32_t)(header.bfSize - header.bfOffBits);

    std::vector<uint8_t> data(header.biSizeImage);
    file.seekg(header.bfOffBits, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), header.biSizeImage);

    if (!file) {
        std::cerr << "Error reading image data: " << strerror(errno) << "\n";
        throw std::runtime_error("Error reading image data");
    }

    // BGR to RGB
    if (header.biBitCount == 24) {
        for (int y = 0; y < header.biHeight; ++y) {
            for (int x = 0; x < header.biWidth; ++x) {
                int index = (y * header.biWidth + x) * 3;
                std::swap(data[index], data[index + 2]);
            }
        }
    }

    return {header, data, colorTable};
}

void writeBMP(const std::string &filename, const Image &image) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error writing file: " << strerror(errno) << "\n";
        throw std::runtime_error("Error writing file");
    }

    file.write(reinterpret_cast<const char*>(&image.header), sizeof(BMPHeader));

    if (image.header.biBitCount == 8) {
        file.write(reinterpret_cast<const char*>(image.colorTable.data()), image.colorTable.size());
    }

    // RGB to BGR
    std::vector<uint8_t> convertedData = image.data;
    if (image.header.biBitCount == 24) {
        for (int y = 0; y < image.header.biHeight; ++y) {
            for (int x = 0; x < image.header.biWidth; ++x) {
                int index = (y * image.header.biWidth + x) * 3;
                std::swap(convertedData[index], convertedData[index + 2]);
            }
        }
    }

    file.write(reinterpret_cast<const char*>(convertedData.data()), convertedData.size());
}

void addSaltAndPepperNoise(Image &image, float noiseLevel) {
    if (noiseLevel < 0.0f || noiseLevel > 1.0f) {
        std::cerr << "Noise level must be between 0 and 1.\n";
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    size_t numPixels;
    size_t numNoisyPixels;

    if (image.header.biBitCount == 24) {
        numPixels = image.data.size() / 3; // 每个像素3字节
        numNoisyPixels = static_cast<size_t>(noiseLevel * numPixels);

        for (size_t i = 0; i < numNoisyPixels; ++i) {
            size_t pixelIndex = static_cast<size_t>(dis(gen) * numPixels);
            size_t dataIndex = pixelIndex * 3; // 每个像素3个字节

            if (dis(gen) < 0.5) {
                // 黑色噪声
                image.data[dataIndex] = 0;
                image.data[dataIndex + 1] = 0;
                image.data[dataIndex + 2] = 0;
            } else {
                // 白色噪声
                image.data[dataIndex] = 255;
                image.data[dataIndex + 1] = 255;
                image.data[dataIndex + 2] = 255;
            }
        }
    } else if (image.header.biBitCount == 8) {
        numPixels = image.data.size(); // 每个像素1字节
        numNoisyPixels = static_cast<size_t>(noiseLevel * numPixels);

        for (size_t i = 0; i < numNoisyPixels; ++i) {
            size_t dataIndex = static_cast<size_t>(dis(gen) * numPixels);

            if (dis(gen) < 0.5) {
                image.data[dataIndex] = 0; // 黑色噪声
            } else {
                image.data[dataIndex] = 255; // 白色噪声
            }
        }
    } else {
        std::cerr << "Unsupported bit count: " << image.header.biBitCount << "\n";
        return;
    }
}

void addRandomNoise(Image &image, float noiseLevel) {
    if (image.data.empty()) {
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::uniform_int_distribution<> index_dis(0, image.data.size() - 1);

    size_t noiseAmount = static_cast<size_t>(image.data.size() * noiseLevel);
    std::unordered_set<size_t> noiseIndices;

    for (size_t i = 0; i < noiseAmount; ++i) {
        size_t index;
        do {
            index = index_dis(gen);
        } while (noiseIndices.find(index) != noiseIndices.end());

        noiseIndices.insert(index);
        image.data[index] = static_cast<uint8_t>(dis(gen));
    }
}

void addGaussianNoise(Image &image, float noiseLevel) {
    if (image.data.empty()) {
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dis(0, 25);
    std::uniform_int_distribution<> index_dis(0, image.data.size() - 1);

    size_t noiseAmount = static_cast<size_t>(image.data.size() * noiseLevel);
    std::unordered_set<size_t> noiseIndices;

    for (size_t i = 0; i < noiseAmount; ++i) {
        size_t index;
        do {
            index = index_dis(gen);
        } while (noiseIndices.find(index) != noiseIndices.end());

        noiseIndices.insert(index);
        int noisyValue = static_cast<int>(image.data[index]) + static_cast<int>(dis(gen));
        noisyValue = std::min(255, std::max(0, noisyValue)); // Clamp to [0, 255]
        image.data[index] = static_cast<uint8_t>(noisyValue);
    }
}

int main() {
    try {
        std::string input_filename, output_filename;
        float noiseLevel;
        int noiseType;

        std::cout << "Enter the input BMP file name: ";
        std::cin >> input_filename;

        Image image = readBMP(input_filename);

        std::cout << "Enter the noise level (0.0 - 1.0): ";
        std::cin >> noiseLevel;

        std::cout << "Select noise type (1 for Salt and Pepper, 2 for Random, 3 for Gaussian): ";
        std::cin >> noiseType;

        if (noiseType == 1) {
            addSaltAndPepperNoise(image, noiseLevel);
        } else if (noiseType == 2) {
            addRandomNoise(image, noiseLevel);
        } else if (noiseType == 3) {
            addGaussianNoise(image, noiseLevel);
        } else {
            std::cerr << "Invalid noise type selected.\n";
            return 1;
        }

        std::cout << "Enter the output BMP file name: ";
        std::cin >> output_filename;

        writeBMP(output_filename, image);

        std::cout << "Noise added and image saved successfully.\n";
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
