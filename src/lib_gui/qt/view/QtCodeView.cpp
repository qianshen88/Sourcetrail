#include "qt/view/QtCodeView.h"

#include "utility/ResourcePaths.h"
#include "utility/tracing.h"

#include "qt/element/QtCodeArea.h"
#include "qt/element/QtCodeNavigator.h"
#include "qt/utility/QtHighlighter.h"
#include "qt/utility/utilityQt.h"
#include "qt/view/QtViewWidgetWrapper.h"
#include "settings/ColorScheme.h"

QtCodeView::QtCodeView(ViewLayout* viewLayout)
	: CodeView(viewLayout)
{
	m_widget = new QtCodeNavigator();
	setStyleSheet();
}

QtCodeView::~QtCodeView()
{
}

void QtCodeView::createWidgetWrapper()
{
	setWidgetWrapper(std::make_shared<QtViewWidgetWrapper>(m_widget));
}

void QtCodeView::initView()
{
}

void QtCodeView::refreshView()
{
	m_onQtThread([=]()
	{
		TRACE("refresh");

		setStyleSheet();

		m_widget->refreshStyle();

		QtCodeArea::clearAnnotationColors();
		QtHighlighter::clearHighlightingRules();
	});
}

bool QtCodeView::isVisible() const
{
	return m_widget->isVisible();
}

void QtCodeView::findMatches(ScreenSearchSender* sender, const std::string& query)
{
	m_onQtThread(
		[sender, query, this]()
		{
			size_t matchCount = m_widget->findScreenMatches(query);
			sender->foundMatches(this, matchCount);
		}
	);
}

void QtCodeView::activateMatch(size_t matchIndex)
{
	m_onQtThread(
		[matchIndex, this]()
		{
			m_widget->activateScreenMatch(matchIndex);
		}
	);
}

void QtCodeView::deactivateMatch(size_t matchIndex)
{
	m_onQtThread(
		[matchIndex, this]()
		{
			m_widget->deactivateScreenMatch(matchIndex);
		}
	);
}

void QtCodeView::clearMatches()
{
	m_onQtThread(
		[this]()
		{
			m_widget->clearScreenMatches();
		}
	);
}

void QtCodeView::clear()
{
	m_onQtThread([=]()
	{
		m_widget->clear();
	});

	m_scrollParams = ScrollParams();
}

bool QtCodeView::showsErrors() const
{
	return m_widget->hasErrors();
}

void QtCodeView::showCodeSnippets(const std::vector<CodeSnippetParams>& snippets, const CodeParams params)
{
	m_onQtThread([=]()
	{
		TRACE("show code snippets");

		if (params.clearSnippets)
		{
			m_widget->clearCodeSnippets();

			m_widget->setActiveTokenIds(params.activeTokenIds);
			m_widget->setErrorInfos(params.errorInfos);

			if (!snippets.size())
			{
				m_widget->clearFile();
			}
		}

		bool addedFiles = false;

		for (const CodeSnippetParams& snippet : snippets)
		{
			if (snippet.isCollapsed)
			{
				m_widget->addFile(snippet.locationFile, snippet.refCount, snippet.modificationTime);

				addedFiles = true;
			}
			else
			{
				m_widget->addCodeSnippet(snippet);
			}
		}

		if (addedFiles)
		{
			m_widget->addedFiles();
		}

		if (params.showContents)
		{
			m_widget->updateFiles();
			m_widget->showContents();
			performScroll();
		}
	});
}

void QtCodeView::scrollTo(const ScrollParams params)
{
	m_scrollParams = params;
}

void QtCodeView::setFileState(const FilePath filePath, FileState state)
{
	m_onQtThread([=]()
	{
		switch (state)
		{
		case FILE_MINIMIZED:
			m_widget->setFileMinimized(filePath);
			break;
		case FILE_SNIPPETS:
			m_widget->setFileSnippets(filePath);
			break;
		case FILE_MAXIMIZED:
			m_widget->setFileMaximized(filePath);
			break;
		}
	});
}

void QtCodeView::showActiveSnippet(
	const std::vector<Id>& activeTokenIds, std::shared_ptr<SourceLocationCollection> collection, bool scrollTo)
{
	m_onQtThread([=]()
	{
		TRACE("show active snippet");
		m_widget->showActiveSnippet(activeTokenIds, collection, scrollTo);
	});
}

void QtCodeView::showActiveTokenIds(const std::vector<Id>& activeTokenIds)
{
	m_onQtThread([=]()
	{
		m_widget->setActiveTokenIds(activeTokenIds);
		m_widget->updateFiles();

		performScroll();
	});
}

void QtCodeView::showActiveLocalSymbolIds(const std::vector<Id>& activeLocalSymbolIds)
{
	m_onQtThread([=]()
	{
		m_widget->setActiveLocalSymbolIds(activeLocalSymbolIds);
		m_widget->updateFiles();
	});
}

void QtCodeView::focusTokenIds(const std::vector<Id>& focusedTokenIds)
{
	m_onQtThread([=]()
	{
		m_widget->focusTokenIds(focusedTokenIds);
	});
}

void QtCodeView::defocusTokenIds()
{
	m_onQtThread([=]()
	{
		m_widget->defocusTokenIds();
	});
}

void QtCodeView::showContents()
{
	m_onQtThread([=]()
	{
		TRACE("show contents");
		m_widget->updateFiles();
		m_widget->showContents();
		performScroll();
	});
}

bool QtCodeView::isInListMode() const
{
	return m_widget->isInListMode();
}

void QtCodeView::setMode(bool listMode)
{
	if (isInListMode() == listMode)
	{
		return;
	}

	m_onQtThread([=]()
	{
		m_widget->setMode(listMode ? QtCodeNavigator::MODE_LIST : QtCodeNavigator::MODE_SINGLE);
	});
}

bool QtCodeView::hasSingleFileCached(const FilePath& filePath) const
{
	return m_widget->hasSingleFileCached(filePath);
}

void QtCodeView::performScroll()
{
	switch (m_scrollParams.type)
	{
	case ScrollParams::SCROLL_TO_DEFINITION:
		m_widget->scrollToDefinition(m_scrollParams.animated, m_scrollParams.ignoreActiveReference);
		break;
	case ScrollParams::SCROLL_TO_LINE:
		m_widget->scrollToLine(m_scrollParams.filePath, m_scrollParams.line);
		break;
	case ScrollParams::SCROLL_TO_VALUE:
		m_widget->scrollToValue(m_scrollParams.value, m_scrollParams.inListMode);
		break;
	default:
		break;
	}

	m_widget->scrollToSnippetIfRequested();

	m_scrollParams = ScrollParams();
}

void QtCodeView::setStyleSheet() const
{
	utility::setWidgetBackgroundColor(m_widget, ColorScheme::getInstance()->getColor("code/background"));

	std::string styleSheet = utility::getStyleSheet(ResourcePaths::getGuiPath().concat(FilePath("code_view/code_view.css")));

	m_widget->setStyleSheet(styleSheet.c_str());
}
