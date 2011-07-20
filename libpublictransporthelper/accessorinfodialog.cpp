/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "accessorinfodialog.h"
#include "ui_accessorInfo.h"
#include <KToolInvocation>
#include <KMessageBox>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class AccessorInfoDialogPrivate {
public:
	AccessorInfoDialogPrivate( const QVariantHash &_serviceProviderData, 
							   AccessorInfoDialog::Options _options ) {
		serviceProviderData = _serviceProviderData;
		options = _options;
	};
	
	Ui::accessorInfo uiAccessorInfo;
	QVariantHash serviceProviderData;
	AccessorInfoDialog::Options options;
};

AccessorInfoDialog::AccessorInfoDialog( const QVariantHash &serviceProviderData, const QIcon &icon,
		AccessorInfoDialog::Options options, QWidget* parent ) 
		: KDialog(parent), d_ptr(new AccessorInfoDialogPrivate(serviceProviderData, options))
{

	QWidget *widget = new QWidget;
	d_ptr->uiAccessorInfo.setupUi( widget );

	setModal( true );
	setButtons( KDialog::Ok );
	setMainWidget( widget );
	setWindowTitle( i18nc( "@title:window", "Service Provider Information" ) );
	setWindowIcon( KIcon( "help-about" ) );

	d_ptr->uiAccessorInfo.icon->setPixmap( icon.pixmap( 32 ) );
	d_ptr->uiAccessorInfo.serviceProviderName->setText(serviceProviderData["name"].toString() );
	d_ptr->uiAccessorInfo.version->setText( i18nc("@info/plain", "Version %1",
			serviceProviderData["version"].toString()) );
	d_ptr->uiAccessorInfo.url->setUrl( serviceProviderData["url"].toString() );
	d_ptr->uiAccessorInfo.url->setText( QString("<a href='%1'>%1</a>").arg(
			serviceProviderData["url"].toString()) );

	d_ptr->uiAccessorInfo.fileName->setUrl( serviceProviderData["fileName"].toString() );
	d_ptr->uiAccessorInfo.fileName->setText( QString("<a href='%1'>%1</a>").arg(
			serviceProviderData["fileName"].toString()) );

	QString scriptFileName = serviceProviderData["scriptFileName"].toString();
	if ( scriptFileName.isEmpty() ) {
		d_ptr->uiAccessorInfo.lblScriptFileName->setVisible( false );
		d_ptr->uiAccessorInfo.scriptFileName->setVisible( false );
	} else {
		d_ptr->uiAccessorInfo.lblScriptFileName->setVisible( true );
		d_ptr->uiAccessorInfo.scriptFileName->setVisible( true );
		d_ptr->uiAccessorInfo.scriptFileName->setUrl( scriptFileName );
		d_ptr->uiAccessorInfo.scriptFileName->setText( QString("<a href='%1'>%1</a>")
				.arg(scriptFileName) );
	}

	if ( serviceProviderData["email"].toString().isEmpty() ) {
		d_ptr->uiAccessorInfo.author->setText( serviceProviderData["author"].toString() );
	} else {
		d_ptr->uiAccessorInfo.author->setText( QString("<a href='mailto:%2'>%1</a> (%3)")
				.arg(serviceProviderData["author"].toString())
				.arg(serviceProviderData["email"].toString())
				.arg(serviceProviderData["shortAuthor"].toString()) );
		d_ptr->uiAccessorInfo.author->setToolTip( i18nc("@info",
				"Write an email to <email address='%2'>%1</email> (%3)",
				serviceProviderData["author"].toString(),
				serviceProviderData["email"].toString(),
				serviceProviderData["shortAuthor"].toString()) );
	}
	d_ptr->uiAccessorInfo.description->setText( serviceProviderData["description"].toString() );
	d_ptr->uiAccessorInfo.features->setText( serviceProviderData["featuresLocalized"].toStringList().join(", ") );

	QStringList changelogEntries = serviceProviderData["changelog"].toStringList();
	if ( changelogEntries.isEmpty() ) {
		d_ptr->uiAccessorInfo.lblChangelog->hide();
		d_ptr->uiAccessorInfo.changelog->hide();
	} else {
		QString changelog("<ul style='margin-left:-20;'>");
		foreach ( const QString &entry, changelogEntries ) {
			int pos = entry.indexOf(':');
			if ( pos == -1 ) {
				changelog.append( QString("<li>%1</li>").arg(entry) );
			} else {
				QString e = entry;
				changelog.append( QString("<li><span style='font-style: italic;'>%1</li>")
						.arg(e.insert(pos + 1, QLatin1String("</span>"))) );
			}
		}
		changelog.append( QLatin1String("</ul>") );
		d_ptr->uiAccessorInfo.changelog->setHtml( changelog );
	}
	
	if ( options.testFlag(ShowOpenInTimetableMateButton) ) {
		connect( d_ptr->uiAccessorInfo.btnOpenInTimetableMate, SIGNAL(clicked()),
				this, SLOT(openInTimetableMate()) );
	} else {
		d_ptr->uiAccessorInfo.btnOpenInTimetableMate->hide();
	}
}

AccessorInfoDialog::~AccessorInfoDialog()
{
	delete d_ptr;
}

void AccessorInfoDialog::openInTimetableMate()
{
	Q_D( AccessorInfoDialog );
	QString error;
	int result = KToolInvocation::startServiceByDesktopName( "timetablemate",
			d->serviceProviderData["fileName"].toString(), &error );
	if ( result != 0 ) {
		KMessageBox::error( this, i18nc("@info",
				"TimetableMate couldn't be started, error message was: '%1'", error) );
	}
}

}; // namespace Timetable
