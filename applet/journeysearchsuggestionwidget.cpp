/*
 *   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#include "journeysearchsuggestionwidget.h"
#include "journeysearchparser.h"
#include "settings.h"
#include "htmldelegate.h"

#include <QStandardItemModel>
#include <QTreeView>

#include <KColorScheme>
#include <KDebug>
#include <Plasma/LineEdit>

JourneySearchSuggestionWidget::JourneySearchSuggestionWidget( Settings *settings,
															  const QPalette &palette )
		: m_settings(settings), m_lineEdit(0)
{
	m_journeySearchLastTextLength = 0;
	m_enabledSuggestions = AllSuggestions;

	m_model = new QStandardItemModel( this );
	setModel( m_model );

	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	setFont( settings->sizedFont() );

	QTreeView *treeView = nativeWidget();
	treeView->setRootIsDecorated( false );
	treeView->setHeaderHidden( true );
	treeView->setAlternatingRowColors( true );
	treeView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	treeView->setAutoFillBackground( false );
	treeView->setAttribute( Qt::WA_NoSystemBackground );
	treeView->setItemDelegate( new PublicTransportDelegate(this) );
	treeView->setPalette( palette );

	connect( treeView, SIGNAL(clicked(QModelIndex)),
			 this, SLOT(suggestionClicked(QModelIndex)) );
	connect( treeView, SIGNAL(doubleClicked(QModelIndex)),
			 this, SLOT(suggestionDoubleClicked(QModelIndex)) );

	// Add recent journey suggestions.
	// Doesn't need attached line edit here, because it's normally empty at this time.
	// If not, it gets updated once a line edit gets attached
	addJourneySearchCompletions();
}

void JourneySearchSuggestionWidget::attachLineEdit(Plasma::LineEdit* lineEdit)
{
	m_lineEdit = lineEdit;
	connect( lineEdit, SIGNAL(textEdited(QString)), this, SLOT(journeySearchLineEdited(QString)) );

	if ( !lineEdit->text().isEmpty() ) {
		// TODO
		clear();
		addJourneySearchCompletions();
	}
}

void JourneySearchSuggestionWidget::detachLineEdit()
{
	disconnect( m_lineEdit, SIGNAL(textEdited(QString)), this, SLOT(journeySearchLineEdited(QString)) );
	m_lineEdit = NULL;
}

void JourneySearchSuggestionWidget::clear()
{
	m_model->clear();
}

void JourneySearchSuggestionWidget::removeGeneralSuggestionItems()
{
	if ( (m_lineEdit && m_lineEdit->text().isEmpty()) || !m_model ) {
		return;
	}

	// Remove previously added suggestion items
	QModelIndexList indices = m_model->match( m_model->index(0, 0),
											  Qt::UserRole + 5, true, -1, Qt::MatchExactly );
	for ( int i = indices.count() - 1; i >= 0; --i ) {
		m_model->removeRow( indices.at(i).row() );
	}
}

void JourneySearchSuggestionWidget::addJourneySearchCompletions()
{
// 	if ( !m_lineEdit ) {
// 		kDebug() << "You need to attach a line edit before calling addJourneySearchCompletions";
// 		return;
// 	}

	// Insert journey search completions to the top of the list
	int row = 0;

	// Add recent journey searches
	int recentCount = 0;
	if ( m_enabledSuggestions.testFlag(RecentJourneySearchSuggestion) ) {
		if ( m_lineEdit ) {
			foreach( const QString &recent, m_settings->recentJourneySearches ) {
				int posStart, len;
				QString stop;
				JourneySearchParser::stopNamePosition( m_lineEdit->nativeWidget(), &posStart, &len, &stop );
				if ( recent.contains(stop) ) {
					QStandardItem *item = new QStandardItem( KIcon( "emblem-favorite" ),
							i18nc("@item:inlistbox/rich", "<emphasis strong='1'>Recent:</emphasis> %1", recent) );
					item->setData( "recent", Qt::UserRole + 1 );
					item->setData( recent, Qt::UserRole + 2 );
					item->setData( true, Qt::UserRole + 5 ); // Mark as suggestion item to easily remove it again
					m_model->insertRow( row, item );
					++row;

					++recentCount;
					if ( recentCount == 5 ) {
						break; // Only show the last five recent journey searches
					}
				}
			}
		} else {
			foreach( const QString &recent, m_settings->recentJourneySearches ) {
				QStandardItem *item = new QStandardItem( KIcon( "emblem-favorite" ),
						i18nc("@item:inlistbox/rich", "<emphasis strong='1'>Recent:</emphasis> %1", recent) );
				item->setData( "recent", Qt::UserRole + 1 );
				item->setData( recent, Qt::UserRole + 2 );
				item->setData( true, Qt::UserRole + 5 ); // Mark as suggestion item to easily remove it again
				m_model->insertRow( row, item );
				++row;

				++recentCount;
				if ( recentCount == 5 ) {
					break; // Only show the last five recent journey searches
				}
			}
		}
	}

	// Add other suggestions
	if ( m_enabledSuggestions.testFlag(KeywordSuggestion) ) {
		if ( m_lineEdit && !m_lineEdit->text().isEmpty() ) {
			QStringList suggestions, suggestionValues;
			// Check if there's already an "at" or "in" (time) keyword
			QStringList words = JourneySearchParser::notDoubleQuotedWords( m_lineEdit->text() );
			QString timeKeywordIn, timeKeywordAt;
			if ( !JourneySearchParser::timeKeywordsIn().isEmpty() ) {
				timeKeywordIn = JourneySearchParser::timeKeywordsIn().first();
			}
			if ( !JourneySearchParser::timeKeywordsAt().isEmpty() ) {
				timeKeywordAt = JourneySearchParser::timeKeywordsAt().first();
			}
			bool hasInKeyword = words.contains( timeKeywordIn );
			bool hasAtKeyword = words.contains( timeKeywordAt );
			bool hasTimeKeyword = hasInKeyword || hasAtKeyword;

			QString type;
			QString extraRegExp;
			if ( hasTimeKeyword ) {
				type = "replaceTimeKeyword";

				QHash<JourneySearchParser::Keyword, QVariant> keywordValues =
				JourneySearchParser::keywordValues( m_lineEdit->text() );
				if ( keywordValues.contains( JourneySearchParser::KeywordTimeAt ) ) {
					QDateTime dateTime = keywordValues[ JourneySearchParser::KeywordTimeAt ].toDateTime();
					extraRegExp = "(\\d{2}:\\d{2}|\\d{2}\\.\\d{2}(\\.\\d{2,4}))";

					// Add "30 minutes later"
					suggestions << i18nc( "@item:inlistbox/rich", "%1 minutes later", 30 );
					suggestionValues << QString( "%1 %2" ).arg( timeKeywordAt )
							.arg( KGlobal::locale()->formatTime( dateTime.addSecs(30 * 60).time() ) );

					// Add "60 minutes later"
					suggestions << i18nc( "@item:inlistbox/rich", "%1 minutes later", 60 );
					suggestionValues << QString( "%1 %2" ).arg( timeKeywordAt )
							.arg( KGlobal::locale()->formatTime( dateTime.addSecs(60 * 60).time() ) );

					// Add "30 minutes earlier"
					suggestions << i18nc( "@item:inlistbox/rich", "%1 minutes earlier", 30 );
					suggestionValues << QString( "%1 %2" ).arg( timeKeywordAt )
							.arg( KGlobal::locale()->formatTime( dateTime.addSecs(-30 * 60).time() ) );
				} else if ( keywordValues.contains( JourneySearchParser::KeywordTimeIn ) ) {
					int minutes = keywordValues[ JourneySearchParser::KeywordTimeIn ].toInt();
					extraRegExp = JourneySearchParser::relativeTimeString( "\\d{1,}" );

					// Add "30 minutes later"
					suggestions << i18nc( "@item:inlistbox/rich", "%1 minutes later", 30 );
					suggestionValues << QString("%1 %2").arg( timeKeywordIn )
							.arg( JourneySearchParser::relativeTimeString(minutes + 30) );

					// Add "60 minutes later"
					suggestions << i18nc( "@item:inlistbox/rich", "%1 minutes later", 60 );
					suggestionValues << QString("%1 %2").arg( timeKeywordIn )
							.arg( JourneySearchParser::relativeTimeString(minutes + 60) );

					// Add "30 minutes earlier"
					suggestions << i18nc( "@item:inlistbox/rich", "%1 minutes earlier", 30 );
					suggestionValues << QString("%1 %2").arg( timeKeywordIn )
							.arg( JourneySearchParser::relativeTimeString(minutes - 30) );
				}
			} else {
				type = "additionalKeywordAtEnd";

				// Use the first keyword of some types for suggestions
				if ( !timeKeywordIn.isEmpty() ) {
					// Add "in 5 minutes"
					QString in5Minutes = QString("%1 %2").arg( timeKeywordIn )
							.arg( JourneySearchParser::relativeTimeString(5) );
					suggestions << in5Minutes;
					suggestionValues << in5Minutes;

					// Add "in 15 minutes"
					QString in15Minutes = QString("%1 %2").arg( timeKeywordIn )
							.arg( JourneySearchParser::relativeTimeString(15) );
					suggestions << in15Minutes;
					suggestionValues << in15Minutes;

					// Add "in 30 minutes"
					QString in30Minutes = QString("%1 %2").arg( timeKeywordIn )
							.arg( JourneySearchParser::relativeTimeString(30) );
					suggestions << in30Minutes;
					suggestionValues << in30Minutes;
				}
				if ( !timeKeywordAt.isEmpty() ) {
					QString timeKeywordTomorrow;
					if ( !JourneySearchParser::timeKeywordsTomorrow().isEmpty() ) {
						timeKeywordTomorrow = JourneySearchParser::timeKeywordsTomorrow().first();
					}

					if ( timeKeywordTomorrow.isNull() ) {
						// Add "at 6:00"
						QString at6 = QString( "%1 %2" ).arg( timeKeywordAt )
								.arg( QTime( 6, 0 ).toString( "hh:mm" ) );
						suggestions << at6;
						suggestionValues << at6;
					} else {
						// Add "tomorrow at 6:00"
						QString tomorrowAt6 = QString( "%1 %2 %3" ).arg( timeKeywordTomorrow )
								.arg( timeKeywordAt ).arg( QTime( 6, 0 ).toString( "hh:mm" ) );
						suggestions << tomorrowAt6;
						suggestionValues << tomorrowAt6;
					}
				}
			}

			// Add all suggestions
			for ( int i = 0; i < suggestions.count(); ++i ) {
				QStandardItem *item = new QStandardItem( KIcon("chronometer"),
						i18nc("@item:inlistbox/rich", "<emphasis strong='1'>Suggestion:</emphasis> %1",
						suggestions.at(i)) );
				item->setData( type, Qt::UserRole + 1 );
				item->setData( suggestionValues.at(i), Qt::UserRole + 2 );
				if ( !extraRegExp.isNull() ) {
					item->setData( extraRegExp, Qt::UserRole + 3 );
				}
				item->setData( true, Qt::UserRole + 5 ); // Mark as suggestion item to easily remove it again
				m_model->insertRow( row, item );
				++row;
			}
		}
	}
}

void JourneySearchSuggestionWidget::addStopSuggestionItems(const QStringList& stopSuggestions)
{
	if ( !m_enabledSuggestions.testFlag(StopNameSuggestion) ) {
		return;
	}

	foreach( const QString &stop, stopSuggestions ) {
		m_model->appendRow( new QStandardItem(KIcon("public-transport-stop"), stop) );
	}
}

void JourneySearchSuggestionWidget::addAllKeywordAddRemoveItems()
{
	if ( !m_lineEdit ) {
		kDebug() << "You need to attach a line edit before calling addAllKeywordAddRemoveItems";
		return;
	}
	if ( m_lineEdit->text().isEmpty() || !m_model ) {
		return;
	}

	QStringList words = JourneySearchParser::notDoubleQuotedWords( m_lineEdit->text() );
	QString timeKeywordIn, timeKeywordAt, arrivalKeyword, departureKeyword,
	toKeyword, fromKeyword;

	// Use the first keyword of each type for keyword suggestions
	if ( !JourneySearchParser::timeKeywordsIn().isEmpty() ) {
		timeKeywordIn = JourneySearchParser::timeKeywordsIn().first();
	}
	if ( !JourneySearchParser::timeKeywordsAt().isEmpty() ) {
		timeKeywordAt = JourneySearchParser::timeKeywordsAt().first();
	}
	if ( !JourneySearchParser::arrivalKeywords().isEmpty() ) {
		arrivalKeyword = JourneySearchParser::arrivalKeywords().first();
	}
	if ( !JourneySearchParser::departureKeywords().isEmpty() ) {
		departureKeyword = JourneySearchParser::departureKeywords().first();
	}
	if ( !JourneySearchParser::toKeywords().isEmpty() ) {
		toKeyword = JourneySearchParser::toKeywords().first();
	}
	if ( !JourneySearchParser::fromKeywords().isEmpty() ) {
		fromKeyword = JourneySearchParser::fromKeywords().first();
	}

	// Add keyword suggestions, ie. keyword add/remove items
	maybeAddKeywordAddRemoveItems( words,
			QStringList() << toKeyword << fromKeyword,
			"additionalKeywordAtBegin",
			QStringList() << i18nc("@info Description for the 'to' keyword",
								   "Get journeys to the given stop")
						  << i18nc("@info Description for the 'from' keyword",
								   "Get journeys from the given stop") );
	maybeAddKeywordAddRemoveItems( words,
			QStringList() << arrivalKeyword << departureKeyword,
			"additionalKeywordAlmostAtEnd",
			QStringList() << i18nc("@info Description for the 'departing' keyword",
								   "Get journeys departing at the given date/time")
						  << i18nc("@info Description for the 'arriving' keyword",
								   "Get journeys arriving at the given date/time") );
	maybeAddKeywordAddRemoveItems( words,
			QStringList() << timeKeywordAt << timeKeywordIn,
			"additionalKeywordAtEnd",
			QStringList() << i18nc("@info Description for the 'at' keyword",
								   "Specify the departure/arrival time, eg. "
								   "\"%1 12:00, 20.04.2010\"", timeKeywordAt)
						  << i18nc("@info Description for the 'in' keyword",
								   "Specify the departure/arrival time, eg. \"%1 %2\"",
								   timeKeywordIn, JourneySearchParser::relativeTimeString()),
			QStringList() << "(\\d{2}:\\d{2}|\\d{2}\\.\\d{2}(\\.\\d{2,4}))"
						  << JourneySearchParser::relativeTimeString("\\d{1,}") );
}

void JourneySearchSuggestionWidget::maybeAddKeywordAddRemoveItems(const QStringList& words,
		const QStringList& keywords, const QString& type, const QStringList& descriptions,
		const QStringList& extraRegExps)
{
	bool added = false;
	QList<QStandardItem*> addItems;
	for ( int i = 0; i < keywords.count(); ++i ) {
		const QString keyword = keywords.at( i );
		QString description = descriptions.at( i );
		QString extraRegExp;
		if ( i < extraRegExps.count() ) {
			extraRegExp = extraRegExps.at( i );
		}

		QStandardItem *item = NULL;
		QColor keywordColor = KColorScheme( QPalette::Active )
				.foreground( KColorScheme::PositiveText ).color();

		if ( words.contains( keyword, Qt::CaseInsensitive ) ) {
			// Keyword found, add a "remove keyword" item
			item = new QStandardItem( KIcon("list-remove"),
					i18nc("@item:inlistbox/rich",
						  "<emphasis strong='1'>Remove Keyword: "
						  "<span style='color:%3;'>%1</span></emphasis><nl/>%2",
						  keyword, description, keywordColor.name()) );
			item->setData( type + "Remove", Qt::UserRole + 1 );
			if ( !extraRegExp.isNull() ) {
				item->setData( extraRegExp, Qt::UserRole + 3 );
			}
			m_model->appendRow( item );
			added = true;
		} else if ( !added ) {
			// Keyword not found, add an "add keyword" item if no "remove keyword"
			// items will get added
			item = new QStandardItem( KIcon("list-add"),
					i18nc("@item:inlistbox/rich",
						  "<emphasis strong='1'>Add Keyword: "
						  "<span style='color:%3;'>%1</span></emphasis><nl/>%2",
						  keyword, description, keywordColor.name()) );
			item->setData( type, Qt::UserRole + 1 );
			addItems << item;
		}

		if ( item ) {
			item->setData( keyword, Qt::UserRole + 2 ); // Store the keyword
			item->setData( true, Qt::UserRole + 5 ); // Mark as suggestion item to easily remove it again
			item->setData( 2, LinesPerRowRole ); // The description is displayed in the second row
		}
	}

	// Only add "add keyword" items if no "remove keyword" items have been added
	// for this type, because only one keyword of each type is allowed
	if ( !added ) {
		foreach( QStandardItem *item, addItems ) {
			m_model->appendRow( item );
		}
	} else {
		qDeleteAll( addItems );
	}
}

void JourneySearchSuggestionWidget::journeySearchItemCompleted(const QString& newJourneySearch,
		const QModelIndex& index, int newCursorPos)
{
	if ( !m_lineEdit ) {
		kDebug() << "You need to attach a line edit first";
		return;
	}
	if ( index.isValid() ) {
		m_model->removeRow( index.row() );
	}
	m_lineEdit->setText( newJourneySearch );

	// For autocompletion and to update suggestions
// 	journeySearchInputEdited( journeySearch->text() ); TEST should be called automatically

	if ( newCursorPos != -1 ) {
		m_lineEdit->nativeWidget()->setCursorPosition( newCursorPos );
	}
}

void JourneySearchSuggestionWidget::useStopSuggestion(const QModelIndex& index)
{
	// Only start search if a stop suggestion or a recent item was activated
	if ( !index.data(Qt::UserRole + 1).isValid()
		|| index.data(Qt::UserRole + 1).toString() == "recent" )
	{
		suggestionClicked( index );
	}
}

void JourneySearchSuggestionWidget::suggestionClicked(const QModelIndex& index)
{
	if ( !m_lineEdit ) {
		kDebug() << "You need to attach a line edit first";
		return;
	}

	QString type = index.data( Qt::UserRole + 1 ).toString();
	if ( type == "recent" ) {
		// Set recent journey search string
		QString newText = index.data( Qt::UserRole + 2 ).toString();
		m_lineEdit->setText( newText );
		removeGeneralSuggestionItems();
		addJourneySearchCompletions();
		addAllKeywordAddRemoveItems();
	} else if ( type == "additionalKeywordAtEnd" ) {
		// Add keyword at the endint newCursorPos
		QString newText = m_lineEdit->text() + ' ' +
		index.data( Qt::UserRole + 2 ).toString();
		journeySearchItemCompleted( newText, index );
	} else if ( type == "additionalKeywordAlmostAtEnd" ) {
		// Add keyword after the stop name, if any
		int posStart, len;
		QString newText, keyword = index.data( Qt::UserRole + 2 ).toString();
		JourneySearchParser::stopNamePosition( m_lineEdit->nativeWidget(), &posStart, &len );
		if ( posStart != -1 ) {
			newText = m_lineEdit->text().insert( posStart + len, ' ' + keyword );
			journeySearchItemCompleted( newText, index, posStart + len + keyword.length() + 1 );
		} else {
			newText = m_lineEdit->text() + ' ' + keyword;
			journeySearchItemCompleted( newText, index );
		}
	} else if ( type == "additionalKeywordAtBegin" ) {
		// Add keyword to the beginning
		QString keyword = index.data( Qt::UserRole + 2 ).toString();
		QString newText = keyword + ' ' + m_lineEdit->text();
		journeySearchItemCompleted( newText, index, keyword.length() + 1 );
	} else if ( type == "additionalKeywordAtEndRemove"
		|| type == "additionalKeywordAlmostAtEndRemove"
		|| type == "replaceTimeKeyword" )
	{
		// Remove first keyword appearance after the stop name, if any
		QString keyword = index.data( Qt::UserRole + 2 ).toString();
		if ( type == "replaceTimeKeyword" )
			keyword = keyword.left( keyword.indexOf( ' ' ) );

		QString pattern;
		if ( index.data( Qt::UserRole + 3 ).isValid() ) {
			// Use "extra reg exp" to also match values for keywords, eg. "[in] 5 minutes"
			QString extraRegExp = index.data( Qt::UserRole + 3 ).toString();
			pattern = QString( "(?:\"[^\"]*\".*)?(\\s%1\\s%2)" ).arg( keyword ).arg( extraRegExp );
		} else {
			pattern = QString( "(?:\"[^\"]*\".*)?(\\s%1)" ).arg( keyword );
		}

		QRegExp regExp( pattern, Qt::CaseInsensitive );
		regExp.setMinimal( true );
		QString newText = m_lineEdit->text().trimmed();
		int pos = regExp.lastIndexIn( newText );
		if ( pos != -1 ) {
			// Keyword (and values) found, remove
			newText = newText.remove( regExp.pos( 1 ), regExp.cap( 1 ).length() );
			if ( type == "replaceTimeKeyword" ) { // Add new time keyword
				newText = newText + ' ' + index.data( Qt::UserRole + 2 ).toString();
				journeySearchItemCompleted( newText, index );
			} else {
				journeySearchItemCompleted( newText, index, regExp.pos(1) );
			}
		}
	} else if ( type == "additionalKeywordAtBeginRemove" ) {
		// Remove keyword from the beginning
		QString keyword = index.data( Qt::UserRole + 2 ).toString();
		QString newText = m_lineEdit->text().trimmed().remove(
			QRegExp( QString( "^%1\\s" ).arg( keyword ), Qt::CaseInsensitive ) );
		journeySearchItemCompleted( newText, index );
	} else {
		// Insert the clicked stop into the journey search line,
		// don't override keywords and other text
		int posStart, len;
		JourneySearchParser::stopNamePosition( m_lineEdit->nativeWidget(), &posStart, &len );

		QString quotedStop = "\"" + index.data().toString() + "\"";
		if ( posStart == -1 ) {
			// No stop name found
			m_lineEdit->setText( quotedStop );
		} else {
			m_lineEdit->setText( m_lineEdit->text().replace( posStart, len, quotedStop ) );
			m_lineEdit->nativeWidget()->setCursorPosition( posStart + quotedStop.length() );
		}

		// Update suggestions
		removeGeneralSuggestionItems();
		addJourneySearchCompletions();
		addAllKeywordAddRemoveItems();
	}
	m_lineEdit->setFocus();
}

void JourneySearchSuggestionWidget::suggestionDoubleClicked(const QModelIndex& index)
{
	// Only start search if a stop suggestion or a recent item was double clicked
	if ( !index.data(Qt::UserRole + 1).isValid()
		|| index.data(Qt::UserRole + 1).toString() == "recent" )
	{
		emit suggestionActivated();
	}
}

void JourneySearchSuggestionWidget::journeySearchLineEdited(const QString& newText)
{
	QString stop;
	QDateTime departure;
	bool stopIsTarget, timeIsDeparture;

	removeGeneralSuggestionItems();
	addJourneySearchCompletions();
	addAllKeywordAddRemoveItems();

	// Only correct the input string if letters were added (eg. not after pressing backspace).
	m_lettersAddedToJourneySearchLine = newText.length() > m_journeySearchLastTextLength;

	JourneySearchParser::parseJourneySearch( m_lineEdit->nativeWidget(),
											 newText, &stop, &departure, &stopIsTarget, &timeIsDeparture,
											 0, 0, m_lettersAddedToJourneySearchLine );
	m_journeySearchLastTextLength = m_lineEdit->text().length()
			- m_lineEdit->nativeWidget()->selectedText().length();

// 	reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture, true );
	emit journeySearchLineChanged( stop, departure, stopIsTarget, timeIsDeparture );
}

void JourneySearchSuggestionWidget::updateStopSuggestionItems(const QVariantHash& stopSuggestionData)
{
	// First read the data from stopSuggestionData
	QVariantHash stopToStopID;
	QHash< QString, int > stopToStopWeight;
	QStringList stopSuggestions, weightedStops;

	// Get all stop names, IDs, weights
	int count = stopSuggestionData["count"].toInt(); // The number of received stop suggestions
	bool hasAtLeastOneWeight = false;
	for ( int i = 0; i < count; ++i ) {
		if ( !stopSuggestionData.contains(QString("stopName %1").arg(i)) ) {
			kDebug() << "doesn't contain 'stopName" << i << "'! count ="
					 << count << "data =" << stopSuggestionData;
			break;
		}

		// Each stop suggestion is stored as a hash in a key named "stopName X",
		// where X is the index of the stop suggestion
		QVariantHash dataMap = stopSuggestionData.value( QString("stopName %1").arg(i) ).toHash();
		QString sStopName = dataMap["stopName"].toString();
		QString sStopID = dataMap["stopID"].toString();
		int stopWeight = dataMap["stopWeight"].toInt();
		stopSuggestions << sStopName;
		if ( stopWeight <= 0 ) {
			stopWeight = 0;
		} else {
			hasAtLeastOneWeight = true;
		}
		weightedStops << QString( "%1:%2" ).arg( sStopName ).arg( stopWeight );

		stopToStopID.insert( sStopName, sStopID );
		stopToStopWeight.insert( sStopName, stopWeight );
	}

	// Set commpletion items
	if ( m_lineEdit && m_lettersAddedToJourneySearchLine
		&& m_lineEdit->nativeWidget()->completionMode() !=
		KGlobalSettings::CompletionNone )
	{
		int posStart, len;
		QString stopName;
		KLineEdit *lineEdit = m_lineEdit->nativeWidget();
		JourneySearchParser::stopNamePosition( m_lineEdit->nativeWidget(),
											&posStart, &len, &stopName );
		int selStart = lineEdit->selectionStart();
		if ( selStart == -1 ) {
			selStart = lineEdit->cursorPosition();
		}
		bool stopNameChanged = selStart > posStart
		&& selStart + lineEdit->selectedText().length()
				<= posStart + len;
		if ( stopNameChanged ) {
			KCompletion *comp = lineEdit->completionObject( false );
			comp->setIgnoreCase( true );
			if ( hasAtLeastOneWeight ) {
				comp->setOrder( KCompletion::Weighted );
				comp->setItems( weightedStops );
			} else {
				comp->setItems( stopSuggestions );
			}
			QString completion = comp->makeCompletion( stopName );

			if ( completion != stopName ) {
				JourneySearchParser::setJourneySearchStopNameCompletion( lineEdit, completion );
			}
		}
	}

	// Update model
	clear();
	removeGeneralSuggestionItems(); // TODO: Does this make sense after clear()?!?
	addJourneySearchCompletions();
	addStopSuggestionItems( stopSuggestions );
	addAllKeywordAddRemoveItems();
}
