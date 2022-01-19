#include "options_dialog.h"
#include "mainwindow.h"  // TODO move StateTransaction to Backend
#include "backend.h"
#include "lib/layout_macros.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSpinBox>

#include <QBoxLayout>
#include <QFormLayout>

class OptionsDialogImpl : public OptionsDialog {
private:
    MainWindow * _main;
    Backend * _backend;
    AppSettings _app;

    QSpinBox * _sample_rate;
    QCheckBox * _use_chip_rate;

    QPushButton * _ok;
    QPushButton * _cancel;

public:
    OptionsDialogImpl(Backend * backend, MainWindow * parent_main)
        : OptionsDialog(parent_main)
        , _main(parent_main)
        , _backend(backend)
        , _app(_backend->settings().app_settings())
    {
        setModal(true);
        setWindowTitle(tr("Options"));

        // Hide contextual-help button in the title bar.
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        auto l = new QVBoxLayout(this);
        {l__form(QFormLayout);
            {form__label_w(tr("Default sample rate:"), QSpinBox);
                _sample_rate = w;
                w->setRange(1, 10'000'000);
            }
            {form__w(QCheckBox(tr("Use native chip sample rate")));
                _use_chip_rate = w;
            }
        }
        {l__w(QDialogButtonBox);
            _ok = w->addButton(QDialogButtonBox::Ok);
            _cancel = w->addButton(QDialogButtonBox::Cancel);
        }

        _sample_rate->setValue((int) _app.sample_rate);
        connect(
            _sample_rate, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int sample_rate) {
                _app.sample_rate = (uint32_t) sample_rate;
            });

        _use_chip_rate->setChecked(_app.use_chip_rate);
        connect(
            _use_chip_rate, &QCheckBox::toggled,
            this, [this](bool use_chip_rate) {
                _app.use_chip_rate = use_chip_rate;
            });

        connect(
            _ok, &QPushButton::clicked,
            this, &OptionsDialogImpl::ok);
        connect(
            _cancel, &QPushButton::clicked,
            this, &OptionsDialogImpl::cancel);
    }

    void ok() {
        // Perhaps factor out into apply()?
        auto tx = _main->edit_unwrap();
        _backend->settings_mut(tx).set_app_settings(_app);
        accept();
    }

    void cancel() {
        reject();
    }
};

OptionsDialog * OptionsDialog::make(Backend * backend, MainWindow * parent_main) {
    return new OptionsDialogImpl(backend, parent_main);
}
