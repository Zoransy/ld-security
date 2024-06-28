#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <vector>
#include <QImage>
#include <QLabel>
#include <QTextEdit>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_readImageButton_clicked();
    void on_embedButton_clicked();
    void on_saveImageButton_clicked();
    void on_extractButton_clicked();
    void on_clearButton_clicked();

private:
    Ui::MainWindow *ui;
    QString currentFilePath;
    std::vector<uint8_t> imageData;
    std::vector<uint8_t> colorTable;
    int width;
    int height;
    int bitCount;
    int dataOffset;
    QImage originalImage;
    QImage modifiedImage;

    // Functions for BMP image processing
    std::vector<int> generateRandomSequence(const std::string &key, int length);
    std::string extractMessageWithKey(const std::vector<uint8_t> &imageData, const std::string &key);
    void embedMessageWithKey(std::vector<uint8_t> &imageData, const std::string &message, const std::string &key);
    void embedMessage(std::vector<uint8_t> &imageData, const std::string &message);
    std::string extractMessage(const std::vector<uint8_t> &imageData);
    size_t calculateMaxEmbedLength(const std::vector<uint8_t> &imageData);
    bool readBMP(const QString &filePath);
    bool writeBMP(const QString &filePath, const std::vector<uint8_t> &imageData);
    void displayImage(QLabel *label, const QImage &image);
    void displayImageInfo();

    const uint8_t END_MARKER = 0xFF; // 定义终止符
};

#endif // MAINWINDOW_H
