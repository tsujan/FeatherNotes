#ifndef HELP_H
#define HELP_H

#include <QDialog>
#include <QPushButton>
#include "ui_helpDialog.h"

namespace FeatherNotes {

namespace Ui {
class helpDialog;
}

class FHelp : public QDialog {
  Q_OBJECT

public:
  FHelp (QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QDialog (parent, f), ui (new Ui::helpDialog) {
      ui->setupUi (this);
      QPushButton *closelButton = ui->buttonBox->button (QDialogButtonBox::Close);
      connect (closelButton, &QAbstractButton::clicked, this, &QDialog::reject);
  }
  ~FHelp() { delete ui; }

private:
  Ui::helpDialog *ui;
};

}

#endif // HELP_H
