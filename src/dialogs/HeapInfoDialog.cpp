#include "HeapInfoDialog.h"

#include <utility>
#include "ui_HeapInfoDialog.h"

HeapInfoDialog::HeapInfoDialog(RVA offset, QString status, QWidget *parent)
    : QDialog(parent), ui(new Ui::HeapInfoDialog), offset(offset), status(std::move(status))
{
    ui->setupUi(this);
    updateFields();
}

HeapInfoDialog::~HeapInfoDialog()
{
    delete ui;
}

void HeapInfoDialog::updateFields()
{
    this->setWindowTitle(QString("Chunk @ ") + RAddressString(offset)
                         + QString("(" + status + ")"));
    this->ui->baseEdit->setText(RAddressString(offset));
    RzHeapChunkSimple *chunk = Core()->getHeapChunk(offset);
    this->ui->sizeEdit->setText(RHexString(chunk->size));
    this->ui->bkEdit->setText(RAddressString(chunk->bk));
    this->ui->fdEdit->setText(RAddressString(chunk->fd));
    this->ui->bknsEdit->setText(RAddressString(chunk->bk_nextsize));
    this->ui->fdnsEdit->setText(RAddressString(chunk->fd_nextsize));
    this->ui->prevSizeEdit->setText(RHexString(chunk->prev_size));
    if (chunk->IM) {
        this->ui->rbIM->setChecked(true);
    }
    if (chunk->PI) {
        this->ui->rbPI->setChecked(true);
    }
    if (chunk->NMA) {
        this->ui->rbNMA->setChecked(true);
    }
    free(chunk);
}