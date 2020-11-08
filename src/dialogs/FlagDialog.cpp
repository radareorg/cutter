#include "FlagDialog.h"
#include "ui_FlagDialog.h"

#include <QIntValidator>
#include "core/Cutter.h"


FlagDialog::FlagDialog(RVA offset, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FlagDialog),
    offset(offset)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
    flag = r_flag_get_i(Core()->core()->flags, offset);

    auto size_validator = new QIntValidator(ui->sizeEdit);
    size_validator->setBottom(1);
    ui->sizeEdit->setValidator(size_validator);
    if (flag) {
        ui->nameEdit->setText(flag->name);
        ui->message->setText(tr("Edit flag at %1").arg(RAddressString(offset)));
    } else {
        ui->message->setText(tr("Add flag at %1").arg(RAddressString(offset)));
    }
}

FlagDialog::~FlagDialog() {}

void FlagDialog::on_buttonBox_accepted()
{
    QString name = ui->nameEdit->text().replace(' ', '_');
    RVA size = ui->sizeEdit->text().toULongLong();

    if (flag) {
        Core()->renameFlag(flag->name, name);
        flag->size = size;
    } else {
        Core()->addFlag(offset, name, size);
    }
}

void FlagDialog::on_buttonBox_rejected()
{
    close();
}
