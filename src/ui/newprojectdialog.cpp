#include "newprojectdialog.h"

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
    , m_projectNameEdit(nullptr)
    , m_projectPathEdit(nullptr)
    , m_browseButton(nullptr)
    , m_okButton(nullptr)
    , m_cancelButton(nullptr)
{
    setupUI();
    setupConnections();
    
    // 设置对话框属性
    setWindowTitle("新建项目");
    setFixedSize(500, 200);
    setModal(true);
}

NewProjectDialog::~NewProjectDialog()
{
}

void NewProjectDialog::setupUI()
{
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 项目名称输入区域
    QHBoxLayout *nameLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel("项目名称:", this);
    m_projectNameEdit = new QLineEdit(this);
    m_projectNameEdit->setPlaceholderText("请输入项目名称");
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(m_projectNameEdit);
    
    // 项目路径选择区域
    QHBoxLayout *pathLayout = new QHBoxLayout();
    QLabel *pathLabel = new QLabel("项目路径:", this);
    m_projectPathEdit = new QLineEdit(this);
    m_projectPathEdit->setPlaceholderText("请选择项目文件夹路径");
    m_browseButton = new QPushButton("浏览...", this);
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_projectPathEdit);
    pathLayout->addWidget(m_browseButton);
    
    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("确定", this);
    m_cancelButton = new QPushButton("取消", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    
    // 添加到主布局
    mainLayout->addLayout(nameLayout);
    mainLayout->addLayout(pathLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void NewProjectDialog::setupConnections()
{
    connect(m_browseButton, &QPushButton::clicked, this, &NewProjectDialog::onBrowseClicked);
    connect(m_okButton, &QPushButton::clicked, this, &NewProjectDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &NewProjectDialog::onCancelClicked);
}

QString NewProjectDialog::getProjectName() const
{
    return m_projectNameEdit->text().trimmed();
}

QString NewProjectDialog::getProjectPath() const
{
    return m_projectPathEdit->text().trimmed();
}

void NewProjectDialog::onBrowseClicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, 
        "选择项目文件夹", 
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!dirPath.isEmpty()) {
        m_projectPathEdit->setText(dirPath);
    }
}

void NewProjectDialog::onOkClicked()
{
    QString projectName = getProjectName();
    QString projectPath = getProjectPath();
    
    // 验证输入
    if (projectName.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入项目名称");
        m_projectNameEdit->setFocus();
        return;
    }
    
    if (projectPath.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请选择项目路径");
        m_projectPathEdit->setFocus();
        return;
    }
    
    // 检查路径是否存在
    QDir dir(projectPath);
    if (!dir.exists()) {
        QMessageBox::warning(this, "路径错误", "选择的路径不存在");
        m_projectPathEdit->setFocus();
        return;
    }
    
    // 检查是否已存在同名项目文件夹
    QString projectDir = QDir(projectPath).filePath(projectName);
    if (QDir(projectDir).exists()) {
        QMessageBox::warning(this, "项目已存在", "该路径下已存在同名项目文件夹");
        return;
    }
    
    accept(); // 关闭对话框并返回Accepted
}

void NewProjectDialog::onCancelClicked()
{
    reject(); // 关闭对话框并返回Rejected
}