require 'spec_helper'

module ImageFile
  describe Image, ".new" do
    it "should raise ArgumentError without pixel format" do
      expect {
        described_class.new(width:42, height:42)
      }.to raise_error(ArgumentError)
    end

    it "should raise ArgumentError without width" do
      expect {
        described_class.new(pixel_format: :RGB24, height:42)
      }.to raise_error(ArgumentError)
    end

    it "should raise ArgumentError without height" do
      expect {
        described_class.new(pixel_format: :RGB24, width:42)
      }.to raise_error(ArgumentError)
    end
  end

  describe Image do
    subject { Image.new(width:42, height:42, pixel_format: :RGB24) }

    its(:width) { should be == 42 }
    its(:height) { should be == 42 }
    its(:row_stride) { should be == 42 }
    its(:pixel_format) { should be == :RGB24 }
  end

  describe Image, "with row-stride" do
    subject { Image.new(width:42, height:42, pixel_format: :RGB24, row_stride:64) }

    its(:width) { should be == 42 }
    its(:height) { should be == 42 }
    its(:row_stride) { should be == 64 }
    its(:pixel_format) { should be == :RGB24 }
  end
end

# vim: foldmethod=marker
