
#include <KComboBox>

class CategoryComboBox : public KComboBox {
    public:
	CategoryComboBox( QWidget* parent = 0 ) : KComboBox( parent ) {};
	
	virtual void showPopup();
};