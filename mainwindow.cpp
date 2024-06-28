#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <ctime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
}

MainWindow::~MainWindow() {
    delete ui;
}

std::vector<int> MainWindow::generateRandomSequence(const std::string &key, int length) {
    std::vector<int> sequence(length);
    std::iota(sequence.begin(), sequence.end(), 0);

    std::seed_seq seed(key.begin(), key.end());
    std::mt19937 generator(seed);

    std::shuffle(sequence.begin(), sequence.end(), generator);

    return sequence;
}

void MainWindow::embedMessageWithKey(std::vector<uint8_t> &imageData, const std::string &message, const std::string &key) {
    size_t max_length = calculateMaxEmbedLength(imageData);
    std::vector<uint8_t> encodedMessage;

    for (char c : message) {
        encodedMessage.push_back(static_cast<uint8_t>(c));
    }

    auto sequence = generateRandomSequence(key, imageData.size());

    size_t data_index = 0;
    for (uint8_t byte : encodedMessage) {
        if (data_index + 8 > sequence.size()) {
            break;
        }
        qDebug() << "Embedding byte:" << QString::number(byte, 2).rightJustified(8, '0');
        for (int bit = 0; bit < 8; ++bit) {
            int index = sequence[data_index];
            imageData[index] = (imageData[index] & 0xFE) | ((byte >> bit) & 1);
            ++data_index;
        }
    }

    if (data_index + 8 <= sequence.size()) {
        for (int bit = 0; bit < 8; ++bit) {
            int index = sequence[data_index];
            imageData[index] = (imageData[index] & 0xFE) | ((END_MARKER >> bit) & 1);
            ++data_index;
        }
    }
}

std::string MainWindow::extractMessageWithKey(const std::vector<uint8_t> &imageData, const std::string &key) {
    std::string message;
    auto sequence = generateRandomSequence(key, imageData.size());

    size_t data_index = 0;
    while (data_index + 8 <= sequence.size()) {
        uint8_t byte = 0;
        for (int bit = 0; bit < 8; ++bit) {
            int index = sequence[data_index];
            byte |= (imageData[index] & 1) << bit;
            ++data_index;
        }

        if (byte == END_MARKER) {
            break;
        }

        message.push_back(byte);
    }

    return message;
}


void MainWindow::on_readImageButton_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.bmp)"));
    if (!filePath.isEmpty()) {
        qDebug() << "Selected file path:" << filePath;
        if (readBMP(filePath)) {
            currentFilePath = filePath;
            size_t maxLength = calculateMaxEmbedLength(imageData);
            ui->maxLengthLabel->setText("最大可嵌入信息长度: " + QString::number(maxLength) + " 字节");

            if (bitCount == 24) {
                originalImage = QImage(imageData.data(), width, height, QImage::Format_RGB888);
            } else if (bitCount == 8) {
                originalImage = QImage(imageData.data(), width, height, QImage::Format_Indexed8);
                QVector<QRgb> colorTable;
                for (int i = 0; i < 256; ++i) {
                    colorTable.append(qRgb(i, i, i));
                }
                originalImage.setColorTable(colorTable);
            }
            displayImage(ui->originalImageLabel, originalImage);
            displayImageInfo();
        } else {
            QMessageBox::warning(this, tr("Warning"), tr("Failed to read the image."));
        }
    }
}

void MainWindow::on_embedButton_clicked() {
    if (imageData.empty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please load an image first."));
        return;
    }

    QString message = ui->messageTextEdit->toPlainText();
    size_t maxLength = calculateMaxEmbedLength(imageData);
    if (message.length() > maxLength) {
        QMessageBox::warning(this, tr("Warning"), tr("Message too long to embed."));
        return;
    }

    if (ui->useEncryptionCheckBox->isChecked()) {
        QString key = ui->encryptionKeyLineEdit->text();
        if (key.isEmpty()) {
            QMessageBox::warning(this, tr("Warning"), tr("Please enter an encryption key."));
            return;
        }
        embedMessageWithKey(imageData, message.toStdString(), key.toStdString());
    } else {
        embedMessage(imageData, message.toStdString());
    }

    // 图片用QImage展示，仅展示没有用QImage类的方法处理图像
    if (bitCount == 24) {
        modifiedImage = QImage(imageData.data(), width, height, QImage::Format_RGB888);
    } else if (bitCount == 8) {
        modifiedImage = QImage(imageData.data(), width, height, QImage::Format_Indexed8);
        QVector<QRgb> colorTable;
        for (int i = 0; i < 256; ++i) {
            colorTable.append(qRgb(i, i, i));
        }
        modifiedImage.setColorTable(colorTable);
    }
    displayImage(ui->modifiedImageLabel, modifiedImage);
}

void MainWindow::on_saveImageButton_clicked() {
    if (imageData.empty()) {
        QMessageBox::warning(this, tr("Warning"), tr("No modified image to save."));
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Image"), "", tr("Image Files (*.bmp)"));
    if (!filePath.isEmpty()) {
        if (!writeBMP(filePath, imageData)) {
            QMessageBox::warning(this, tr("Warning"), tr("Failed to save the image."));
        }
    }
}

void MainWindow::on_extractButton_clicked() {
    if (imageData.empty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please load an image first."));
        return;
    }

    std::string message;
    if (ui->useEncryptionCheckBox->isChecked()) {
        QString key = ui->encryptionKeyLineEdit->text();
        if (key.isEmpty()) {
            QMessageBox::warning(this, tr("Warning"), tr("Please enter an encryption key."));
            return;
        }
        message = extractMessageWithKey(imageData, key.toStdString());
    } else {
        message = extractMessage(imageData);
    }

    ui->extractedMessageTextEdit->setPlainText(QString::fromStdString(message));
}

void MainWindow::on_clearButton_clicked() {
    ui->originalImageLabel->clear();
    ui->modifiedImageLabel->clear();
    ui->modifiedImageLabel->setText("嵌入信息后的图片");
    ui->originalImageLabel->setText("原始图片");
    ui->imageInfoLabel->setText("图片信息：宽度，高度，类型");
    ui->maxLengthLabel->setText("最大可嵌入信息长度：");

    ui->messageTextEdit->clear();
    ui->extractedMessageTextEdit->clear();
    ui->encryptionKeyLineEdit->clear();
}

size_t MainWindow::calculateMaxEmbedLength(const std::vector<uint8_t> &imageData) {
    if (bitCount == 24) {
        return (imageData.size() / 3) / 8; // 每个像素有3个通道，计算字节数
    } else if (bitCount == 8) {
        return imageData.size() / 8; // 每个像素一个字节，计算字节数
    }
    return 0;
}

void MainWindow::embedMessage(std::vector<uint8_t> &imageData, const std::string &message) {
    size_t max_length = calculateMaxEmbedLength(imageData);
    std::vector<uint8_t> encodedMessage;

    for (char c : message) {
        encodedMessage.push_back(static_cast<uint8_t>(c));
    }

    size_t data_index = 0;
    for (uint8_t byte : encodedMessage) {
        if (data_index + 8 > imageData.size()) {
            break;
        }
        qDebug() << "Embedding byte:" << QString::number(byte, 2).rightJustified(8, '0');
        for (int bit = 0; bit < 8; ++bit) {
            //qDebug() << "Before: " << QString::number(imageData[data_index], 2).rightJustified(8, '0');
            imageData[data_index] = (imageData[data_index] & 0xFE) | ((byte >> bit) & 1);
            //qDebug() << "After: " << QString::number(imageData[data_index], 2).rightJustified(8, '0');
            ++data_index;
        }
    }

    // 判断是否有空间嵌入文件尾
    if (data_index + 8 <= imageData.size()) {
        qDebug() << "Embedding END_MARKER";
        for (int bit = 0; bit < 8; ++bit) {
            imageData[data_index] = (imageData[data_index] & 0xFE) | ((END_MARKER >> bit) & 1);
            qDebug() << "Embedding END_MARKER byte: " << QString::number(imageData[data_index], 2).rightJustified(8, '0');
            ++data_index;
        }
    } else {
        qDebug() << "No space to embed END_MARKER.";
    }

    // 打印嵌入的信息及其二进制表示
    qDebug() << "Embedded message:" << QString::fromStdString(message);
    qDebug() << "Binary representation:";
    for (uint8_t byte : encodedMessage) {
        //qDebug() << QString::number(byte, 2).rightJustified(8, '0');
    }
}

std::string MainWindow::extractMessage(const std::vector<uint8_t> &imageData) {
    std::string message;
    size_t data_index = 0;

    while (data_index + 8 <= imageData.size()) {
        uint8_t byte = 0;
        for (int bit = 0; bit < 8; ++bit) {
            byte |= (imageData[data_index] & 1) << bit;
            ++data_index;
        }

        //qDebug() << "Extracted byte:" << QString::number(byte, 2).rightJustified(8, '0');

        if (byte == END_MARKER) {
            break;
        }

        message.push_back(byte);
    }

    // 打印提取的信息及其二进制表示
    qDebug() << "Extracted message:" << QString::fromStdString(message);
    qDebug() << "Binary representation:";
    for (char c : message) {
        //qDebug() << QString::number(static_cast<uint8_t>(c), 2).rightJustified(8, '0');
    }

    return message;
}

bool MainWindow::readBMP(const QString &filePath) {
    qDebug() << "Attempting to open file: " << filePath;
    std::ifstream file(filePath.toStdString(), std::ios::binary);
    if (!file) {
        qDebug() << "Error: Unable to open file " << filePath;
        return false;
    }

    // BMP header
    char header[14];
    file.read(header, 14);
    if (file.gcount() != 14) {
        qDebug() << "Error: Unable to read BMP file header";
        return false;
    }

    // DIB header
    char dibHeader[40];
    file.read(dibHeader, 40);
    if (file.gcount() != 40) {
        qDebug() << "Error: Unable to read BMP DIB header";
        return false;
    }

    // 提取信息
    width = *reinterpret_cast<int*>(&dibHeader[4]);
    height = *reinterpret_cast<int*>(&dibHeader[8]);
    bitCount = *reinterpret_cast<short*>(&dibHeader[14]);
    dataOffset = *reinterpret_cast<int*>(&header[10]);

    qDebug() << "Width:" << width << "Height:" << height << "BitCount:" << bitCount << "DataOffset:" << dataOffset;

    // 检查BMP格式是否为24真彩或256灰度图
    if (bitCount != 24 && bitCount != 8) {
        qDebug() << "Error: Unsupported BMP format. Only 24-bit and 8-bit BMP files are supported.";
        return false;
    }

    if (bitCount == 8) {
        colorTable.resize(256 * 4);
        file.read(reinterpret_cast<char*>(colorTable.data()), colorTable.size());
    }

    file.seekg(dataOffset, std::ios::beg);
    imageData.resize(width * height * (bitCount / 8));
    file.read(reinterpret_cast<char*>(imageData.data()), imageData.size());
    if (file.gcount() != static_cast<std::streamsize>(imageData.size())) {
        qDebug() << "Error: Unable to read BMP image data";
        return false;
    }

    // BGR to RGB
    if (bitCount == 24) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 3;
                std::swap(imageData[index], imageData[index + 2]); // BGR to RGB
            }
        }
    }

    return true;
}

bool MainWindow::writeBMP(const QString &filePath, const std::vector<uint8_t> &imageData) {
    std::ofstream file(filePath.toStdString(), std::ios::binary);
    if (!file) {
        qDebug() << "Error: Unable to write file " << filePath;
        return false;
    }

    char header[14] = {
        'B', 'M', // Signature
        0, 0, 0, 0, // File size
        0, 0, 0, 0, // Reserved
        static_cast<char>(dataOffset & 0xFF), static_cast<char>((dataOffset >> 8) & 0xFF),
        static_cast<char>((dataOffset >> 16) & 0xFF), static_cast<char>((dataOffset >> 24) & 0xFF) // Data offset
    };

    uint32_t fileSize = static_cast<uint32_t>(dataOffset + imageData.size());
    header[2] = static_cast<char>(fileSize & 0xFF);
    header[3] = static_cast<char>((fileSize >> 8) & 0xFF);
    header[4] = static_cast<char>((fileSize >> 16) & 0xFF);
    header[5] = static_cast<char>((fileSize >> 24) & 0xFF);

    file.write(header, sizeof(header));

    char dibHeader[40] = {
        40, 0, 0, 0, // Header size
        static_cast<char>(width & 0xFF), static_cast<char>((width >> 8) & 0xFF),
        static_cast<char>((width >> 16) & 0xFF), static_cast<char>((width >> 24) & 0xFF), // Width
        static_cast<char>(height & 0xFF), static_cast<char>((height >> 8) & 0xFF),
        static_cast<char>((height >> 16) & 0xFF), static_cast<char>((height >> 24) & 0xFF), // Height
        1, 0, // Planes
        static_cast<char>(bitCount & 0xFF), static_cast<char>((bitCount >> 8) & 0xFF), // Bit count
        0, 0, 0, 0, // Compression
        0, 0, 0, 0, // Image size
        0, 0, 0, 0, // X pixels per meter
        0, 0, 0, 0, // Y pixels per meter
        0, 0, 0, 0, // Total colors
        0, 0, 0, 0 // Important colors
    };

    file.write(dibHeader, sizeof(dibHeader));

    // color table
    if (bitCount == 8) {
        file.write(reinterpret_cast<const char*>(colorTable.data()), colorTable.size());
    }

    // RGB to BGR
    std::vector<uint8_t> convertedData = imageData;
    if (bitCount == 24) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 3;
                std::swap(convertedData[index], convertedData[index + 2]);
            }
        }
    }

    file.write(reinterpret_cast<const char*>(convertedData.data()), convertedData.size());
    return true;
}


void MainWindow::displayImage(QLabel *label, const QImage &image) {
    // 不知道为什么图像要翻转一下，不然显示的时候是上下颠倒的
    QImage flippedImage = image.mirrored(false, true);
    label->setPixmap(QPixmap::fromImage(flippedImage).scaled(label->size(), Qt::KeepAspectRatio));
}

void MainWindow::displayImageInfo() {
    QString imageInfo = QString("图片信息: 宽度: %1, 高度: %2, 类型: %3")
                            .arg(width)
                            .arg(height)
                            .arg(bitCount == 24 ? "24位真彩图" : "256色度灰度图");
    ui->imageInfoLabel->setText(imageInfo);
}
