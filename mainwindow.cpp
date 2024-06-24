#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <fstream>
#include <vector>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_readImageButton_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.bmp)"));
    if (!filePath.isEmpty()) {
        qDebug() << "Selected file path:" << filePath;
        if (readBMP(filePath)) {
            currentFilePath = filePath;
            size_t maxLength = calculateMaxEmbedLength(imageData);
            ui->maxLengthLabel->setText(QString::number(maxLength));

            // Create a QImage from the processed image data
            originalImage = QImage(imageData.data(), width, height, QImage::Format_RGB888);
            displayImage(ui->originalImageLabel, originalImage);
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

    embedMessage(imageData, message.toStdString());

    // Create a QImage from the modified image data
    modifiedImage = QImage(imageData.data(), width, height, QImage::Format_RGB888);
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

    std::string message = extractMessage(imageData);
    ui->messageTextEdit->setPlainText(QString::fromStdString(message));
}

// Functions for BMP image processing

size_t MainWindow::calculateMaxEmbedLength(const std::vector<uint8_t> &imageData) {
    return (imageData.size() - dataOffset) / 8 - sizeof(size_t);
}

void MainWindow::embedMessage(std::vector<uint8_t> &imageData, const std::string &message) {
    size_t messageLength = message.size();
    // Embed the message length
    for (size_t i = 0; i < sizeof(size_t) * 8; ++i) {
        imageData[dataOffset + i] &= 0xFE; // Clear the least significant bit
        imageData[dataOffset + i] |= (messageLength >> i) & 1; // Set the least significant bit to the length bit
    }

    // Embed the message
    for (size_t i = 0; i < message.size(); ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            imageData[dataOffset + sizeof(size_t) * 8 + i * 8 + bit] &= 0xFE; // Clear the least significant bit
            imageData[dataOffset + sizeof(size_t) * 8 + i * 8 + bit] |= (message[i] >> bit) & 1; // Set the least significant bit to the message bit
        }
    }
}

std::string MainWindow::extractMessage(const std::vector<uint8_t> &imageData) {
    size_t messageLength = 0;
    for (size_t i = 0; i < sizeof(size_t) * 8; ++i) {
        messageLength |= (imageData[dataOffset + i] & 1) << i;
    }

    std::string message(messageLength, 0);
    for (size_t i = 0; i < messageLength; ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            message[i] |= (imageData[dataOffset + sizeof(size_t) * 8 + i * 8 + bit] & 1) << bit;
        }
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

    // Read the BMP file header
    char header[14];
    file.read(header, 14);
    if (file.gcount() != 14) {
        qDebug() << "Error: Unable to read BMP file header";
        return false;
    }

    // Read the DIB header
    char dibHeader[40];
    file.read(dibHeader, 40);
    if (file.gcount() != 40) {
        qDebug() << "Error: Unable to read BMP DIB header";
        return false;
    }

    // Extract information from the headers
    width = *reinterpret_cast<int*>(&dibHeader[4]);
    height = *reinterpret_cast<int*>(&dibHeader[8]);
    bitCount = *reinterpret_cast<short*>(&dibHeader[14]);
    dataOffset = *reinterpret_cast<int*>(&header[10]);

    qDebug() << "Width:" << width << "Height:" << height << "BitCount:" << bitCount << "DataOffset:" << dataOffset;

    // Check if the image format is supported
    if (bitCount != 24) {
        qDebug() << "Error: Unsupported BMP format. Only 24-bit BMP files are supported.";
        return false;
    }

    // Read the image data
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    imageData.resize(fileSize);
    file.read(reinterpret_cast<char*>(imageData.data()), fileSize);
    if (file.gcount() != fileSize) {
        qDebug() << "Error: Unable to read BMP image data";
        return false;
    }

    // Process image data to correct color channel order and flip vertically
    std::vector<uint8_t> processedData(width * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIndex = dataOffset + (y * width + x) * 3;
            int dstIndex = ((height - 1 - y) * width + x) * 3;
            processedData[dstIndex] = imageData[srcIndex + 2];     // R
            processedData[dstIndex + 1] = imageData[srcIndex + 1]; // G
            processedData[dstIndex + 2] = imageData[srcIndex];     // B
        }
    }

    // Replace imageData with processedData
    imageData = std::move(processedData);

    return true;
}


bool MainWindow::writeBMP(const QString &filePath, const std::vector<uint8_t> &imageData) {
    std::ofstream file(filePath.toStdString(), std::ios::binary);
    if (!file) {
        qDebug() << "Error: Unable to write file " << filePath;
        return false;
    }

    file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
    return true;
}

void MainWindow::displayImage(QLabel *label, const QImage &image) {
    label->setPixmap(QPixmap::fromImage(image).scaled(label->size(), Qt::KeepAspectRatio));
}
