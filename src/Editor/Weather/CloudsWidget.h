#pragma once

#include "../Widget.h"

class CloudsWidget : public Widget
{
public:
	CloudsWidget()
	{
		name = "Cloud";
	}

	~CloudsWidget();

	int id;

	CloudsWidget(const CloudsWidget& other)
	{
		name = std::format("{} COPY", other.name);
		id = WidgetSharedData::GetSingleton()->GetNewID();
	}

	virtual Widget* Clone() const override
	{
		return new CloudsWidget(*this);  // Return a copy of this object
	}

	virtual int GetID()
	{
		return id;
	}

	virtual void DrawWidget() override;

	virtual void LoadDefaults() {};
};