require 'spec_helper'

describe "ImageFile module" do
  it "should exists" do
    expect { ImageFile }.to_not raise_error(NameError)
  end
end

module ImageFile
  describe "Image class" do
    it "should exists" do
      expect { Image }.to_not raise_error(NameError)
    end
  end
end
